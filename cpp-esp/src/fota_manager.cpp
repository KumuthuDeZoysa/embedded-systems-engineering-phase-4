#include "../include/fota_manager.hpp"
#include "../include/logger.hpp"
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <ArduinoJson.h>

// Forward declaration of base64_decode
static size_t base64_decode(char* output, const char* input, size_t inputLen);

FOTAManager::FOTAManager(EcoHttpClient* http, SecurityLayer* security, const std::string& cloud_base_url)
    : http_(http)
    , security_(security)
    , cloud_base_url_(cloud_base_url)
    , last_report_ms_(0) {
}

FOTAManager::~FOTAManager() {
    // Cleanup if needed
}

bool FOTAManager::begin() {
    Logger::info("[FOTA] Initializing FOTA Manager");
    
    // Initialize missing files to prevent LittleFS error messages
    if (!LittleFS.exists("/version.txt")) {
        File f = LittleFS.open("/version.txt", "w");
        if (f) {
            f.println("1.0.0-Oct 17 2025-10:39:50");
            f.close();
        }
    }
    
    if (!LittleFS.exists("/boot_count.txt")) {
        File f = LittleFS.open("/boot_count.txt", "w");
        if (f) {
            f.println("0");
            f.close();
        }
    }
    
    if (!LittleFS.exists("/littlefs/fota_state.json")) {
        // Create directory if needed
        if (!LittleFS.exists("/littlefs")) {
            LittleFS.mkdir("/littlefs");
        }
        File f = LittleFS.open("/littlefs/fota_state.json", "w");
        if (f) {
            f.println("{}");
            f.close();
        }
    }
    
    // Check current partition
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running) {
        Logger::info("[FOTA] Running from partition: %s (type=%d, subtype=%d)", 
                    running->label, running->type, running->subtype);
    }
    
    // Load saved state if exists
    if (loadState()) {
        Logger::info("[FOTA] Loaded previous FOTA state: state=%d, chunks=%u/%u",
                    (int)progress_.state, progress_.chunks_received, progress_.total_chunks);
        
        // If we were in progress, check boot count for rollback detection
        if (isInProgress()) {
            int boot_count = getBootCount();
            if (boot_count >= MAX_BOOT_ATTEMPTS) {
                Logger::error("[FOTA] Boot count exceeded (%d), triggering rollback", boot_count);
                rollback("Boot count exceeded after update");
                return false;
            }
            incrementBootCount();
        }
    }
    
    // Get current version
    progress_.current_version = getCurrentFirmwareVersion();
    Logger::info("[FOTA] Current firmware version: %s", progress_.current_version.c_str());
    
    return true;
}

bool FOTAManager::checkForUpdate() {
    Logger::info("[FOTA] Checking for firmware updates");
    setState(FOTAState::CHECKING_MANIFEST);
    
    if (!fetchManifest()) {
        setState(FOTAState::IDLE, "Failed to fetch manifest");
        return false;
    }
    
    if (!manifest_.valid) {
        Logger::info("[FOTA] No firmware update available");
        setState(FOTAState::IDLE);
        return false;
    }
    
    // Check if version is different
    if (manifest_.version == progress_.current_version) {
        Logger::info("[FOTA] Firmware version %s is same as current, no update needed", 
                    manifest_.version.c_str());
        setState(FOTAState::IDLE);
        return false;
    }
    
    Logger::info("[FOTA] New firmware available: %s (current: %s)", 
                manifest_.version.c_str(), progress_.current_version.c_str());
    Logger::info("[FOTA] Size: %u bytes, Chunks: %u, Chunk size: %u", 
                manifest_.size, manifest_.total_chunks, manifest_.chunk_size);
    
    progress_.new_version = manifest_.version;
    progress_.total_chunks = manifest_.total_chunks;
    progress_.total_bytes = manifest_.size;
    setState(FOTAState::IDLE);
    
    logFOTAEvent("manifest_received", 
                "Version: " + manifest_.version + ", Size: " + std::to_string(manifest_.size) + 
                " bytes, Chunks: " + std::to_string(manifest_.total_chunks));
    
    return true;
}

bool FOTAManager::startDownload() {
    if (!manifest_.valid) {
        Logger::error("[FOTA] Cannot start download: no valid manifest");
        return false;
    }
    
    Logger::info("[FOTA] Starting firmware download: version=%s, size=%u", 
                manifest_.version.c_str(), manifest_.size);
    
    setState(FOTAState::DOWNLOADING);
    
    // Initialize chunk tracking
    chunks_downloaded_.clear();
    chunks_downloaded_.resize(manifest_.total_chunks, false);
    
    progress_.chunks_received = 0;
    progress_.bytes_received = 0;
    
    // Clear old firmware file
    clearFirmwareFile();
    
    // Save state
    saveState();
    
    logFOTAEvent("download_started", 
                "Version: " + manifest_.version + ", Total chunks: " + std::to_string(manifest_.total_chunks));
    
    return true;
}

bool FOTAManager::processChunk() {
    if (progress_.state != FOTAState::DOWNLOADING) {
        Logger::warn("[FOTA] processChunk called but not in DOWNLOADING state");
        return false;
    }
    
    // Find next chunk to download with safety checks
    if (manifest_.total_chunks == 0 || chunks_downloaded_.size() != manifest_.total_chunks) {
        Logger::error("[FOTA] Invalid manifest state: total_chunks=%u, vector_size=%u", 
                     manifest_.total_chunks, chunks_downloaded_.size());
        setState(FOTAState::FAILED, "Corrupted manifest state");
        return false;
    }
    
    uint32_t chunk_number = 0;
    bool found = false;
    for (uint32_t i = 0; i < manifest_.total_chunks; i++) {
        if (i >= chunks_downloaded_.size()) {
            Logger::error("[FOTA] Chunk vector access out of bounds: i=%u, size=%u", 
                         i, chunks_downloaded_.size());
            setState(FOTAState::FAILED, "Vector access error");
            return false;
        }
        if (!chunks_downloaded_[i]) {
            chunk_number = i;
            found = true;
            break;
        }
    }
    
    if (!found) {
        Logger::info("[FOTA] All chunks downloaded, proceeding to verification and installation");
        setState(FOTAState::VERIFYING);
        
        // Verify firmware integrity
        if (!verifyFirmware()) {
            setState(FOTAState::FAILED, "Firmware verification failed");
            return false;
        }
        
        // Apply update and reboot
        Logger::info("[FOTA] Firmware verified successfully, applying update...");
        setState(FOTAState::WRITING);
        
        // Small delay before reboot for demo visibility
        delay(2000);
        
        return applyUpdate();
    }
    
    // Download chunk
    if (!fetchChunk(chunk_number)) {
        setState(FOTAState::FAILED, "Failed to fetch chunk " + std::to_string(chunk_number));
        return false;
    }
    
    // Report progress to cloud (re-enabled for demo)
    unsigned long now = millis();
    if (now - last_report_ms_ >= REPORT_INTERVAL_MS) {
        reportProgress(true);
        last_report_ms_ = now;
    }
    
    return true;
}

bool FOTAManager::verifyFirmware() {
    Logger::info("[FOTA] Verifying firmware integrity");
    setState(FOTAState::VERIFYING);
    
    // Load firmware and calculate hash
    File file = LittleFS.open(FIRMWARE_FILE, "r");
    if (!file) {
        setState(FOTAState::FAILED, "Cannot open firmware file for verification");
        return false;
    }
    
    sha256_.reset();
    uint8_t buffer[HASH_BUFFER_SIZE];
    size_t total_read = 0;
    
    while (file.available()) {
        size_t read = file.read(buffer, sizeof(buffer));
        if (read > 0) {
            sha256_.update(buffer, read);
            total_read += read;
        }
    }
    file.close();
    
    if (total_read != manifest_.size) {
        setState(FOTAState::FAILED, 
                "Size mismatch: expected " + std::to_string(manifest_.size) + 
                ", got " + std::to_string(total_read));
        return false;
    }
    
    // Get hash
    uint8_t hash[32];
    sha256_.finalize(hash, sizeof(hash));
    
    // Convert to hex string
    char hash_hex[65];
    for (int i = 0; i < 32; i++) {
        sprintf(hash_hex + (i * 2), "%02x", hash[i]);
    }
    hash_hex[64] = '\0';
    
    // Compare with manifest hash
    if (manifest_.hash != std::string(hash_hex)) {
        setState(FOTAState::FAILED, 
                "Hash mismatch: expected " + manifest_.hash + ", got " + std::string(hash_hex));
        logFOTAEvent("verification_failed", "Hash mismatch");
        return false;
    }
    
    Logger::info("[FOTA] Firmware verification successful: hash=%s", hash_hex);
    progress_.verified = true;
    setState(FOTAState::WRITING);
    
    logFOTAEvent("firmware_verified", "Hash: " + std::string(hash_hex));
    
    return true;
}

bool FOTAManager::applyUpdate() {
    if (!progress_.verified) {
        Logger::error("[FOTA] Cannot apply update: firmware not verified");
        return false;
    }
    
    Logger::info("[FOTA] Applying firmware update");
    setState(FOTAState::WRITING);
    
    // Open firmware file
    File file = LittleFS.open(FIRMWARE_FILE, "r");
    if (!file) {
        setState(FOTAState::FAILED, "Cannot open firmware file");
        return false;
    }
    
    // Begin OTA update
    size_t firmware_size = file.size();
    if (!Update.begin(firmware_size)) {
        file.close();
        String error_msg = "OTA begin failed: " + String(Update.getError());
        setState(FOTAState::FAILED, error_msg.c_str());
        return false;
    }
    
    Logger::info("[FOTA] Writing firmware to OTA partition: %u bytes", firmware_size);
    
    // Write firmware
    uint8_t buffer[HASH_BUFFER_SIZE];
    size_t written = 0;
    
    while (file.available()) {
        size_t read = file.read(buffer, sizeof(buffer));
        if (read > 0) {
            size_t w = Update.write(buffer, read);
            if (w != read) {
                file.close();
                Update.abort();
                setState(FOTAState::FAILED, "Write error: wrote " + std::to_string(w) + 
                        " of " + std::to_string(read) + " bytes");
                return false;
            }
            written += w;
            
            // Log progress every 10KB
            if (written % 10240 == 0 || written == firmware_size) {
                unsigned int pct = 0;
                if (firmware_size > 0) {
                    pct = (unsigned int)((written * 100) / firmware_size);
                }
                Logger::info("[FOTA] Written: %u / %u bytes (%u%%)", 
                            written, firmware_size, pct);
            }
        }
    }
    file.close();
    
    // Finalize update
    if (!Update.end(true)) {
        String error_msg = "OTA end failed: " + String(Update.getError());
        setState(FOTAState::FAILED, error_msg.c_str());
        return false;
    }
    
    Logger::info("[FOTA] Firmware written successfully: %u bytes", written);
    setState(FOTAState::REBOOTING);
    
    // Clear boot count for new firmware
    clearBootCount();
    
    // Save state before reboot
    saveState();
    
    logFOTAEvent("firmware_applied", "Version: " + manifest_.version + ", Size: " + std::to_string(written));
    reportProgress(true);
    
    // Report boot status as "pending"
    {
        DynamicJsonDocument doc(512);
        doc["fota_status"]["boot_status"] = "pending_reboot";
        doc["fota_status"]["new_version"] = manifest_.version;
        std::string json_str;
        serializeJson(doc, json_str);
        
        std::string plain_response;
        http_->post((cloud_base_url_ + "/api/inverter/fota/status").c_str(),
                   json_str.c_str(), json_str.length(), "application/json");
    }
    
    Logger::info("[FOTA] Rebooting in 3 seconds...");
    delay(3000);
    
    ESP.restart();
    
    return true; // Won't reach here
}

bool FOTAManager::rollback(const std::string& reason) {
    Logger::error("[FOTA] Initiating rollback: %s", reason.c_str());
    setState(FOTAState::ROLLBACK, reason);
    
    logFOTAEvent("rollback_triggered", reason);
    
    // Report rollback to cloud
    reportProgress(true);
    
    // Set boot partition to factory or previous OTA
    const esp_partition_t* factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    
    if (factory) {
        Logger::info("[FOTA] Rolling back to factory partition");
        esp_ota_set_boot_partition(factory);
        logFOTAEvent("rollback_to_factory", "Reverting to factory firmware");
    } else {
        // Try to find previous OTA partition
        const esp_partition_t* running = esp_ota_get_running_partition();
        const esp_partition_t* next = esp_ota_get_next_update_partition(running);
        
        if (running && next) {
            Logger::info("[FOTA] Rolling back to previous OTA partition");
            esp_ota_set_boot_partition(next);
            logFOTAEvent("rollback_to_previous_ota", "Reverting to previous OTA partition");
        } else {
            Logger::error("[FOTA] No rollback partition available");
            setState(FOTAState::FAILED, "No rollback partition available");
            return false;
        }
    }
    
    // Clear state
    reset();
    
    Logger::info("[FOTA] Rebooting for rollback in 3 seconds...");
    delay(3000);
    
    ESP.restart();
    
    return true; // Won't reach here
}

bool FOTAManager::reportProgress(bool force) {
    if (!force) {
        unsigned long now = millis();
        if (now - last_report_ms_ < REPORT_INTERVAL_MS) {
            return true; // Skip report
        }
        last_report_ms_ = now;
    }
    
    // Add safety check to prevent crashes
    Logger::debug("[FOTA] reportProgress called - safety check");
    
    // Build progress report
    DynamicJsonDocument doc(1024);
    auto status = doc.createNestedObject("fota_status");
    
    if (progress_.state == FOTAState::DOWNLOADING) {
        status["chunk_received"] = progress_.chunks_received;
        status["total_chunks"] = progress_.total_chunks;
        
        // Add extra safety check for division
        float progress_pct = 0.0f;
        if (progress_.total_chunks > 0 && progress_.chunks_received <= progress_.total_chunks) {
            progress_pct = (float)progress_.chunks_received / progress_.total_chunks * 100.0f;
        }
        status["progress"] = progress_pct;
        
        Logger::info("[FOTA] Progress: %u/%u chunks (%.1f%%)", 
                    progress_.chunks_received, progress_.total_chunks, progress_pct);
    }
    
    if (progress_.state == FOTAState::VERIFYING || progress_.verified) {
        status["verified"] = progress_.verified;
    }
    
    if (progress_.state == FOTAState::ROLLBACK) {
        status["rollback"] = true;
        status["error"] = progress_.error_message.c_str();
    }
    
    if (progress_.state == FOTAState::FAILED) {
        status["error"] = progress_.error_message.c_str();
    }
    
    std::string json_str;
    serializeJson(doc, json_str);
    
    Logger::debug("[FOTA] About to send HTTP POST - JSON size: %d", json_str.length());
    
    // Add try-catch equivalent for ESP32 (using return value checking)
    EcoHttpResponse resp;
    resp.status_code = 0;
    
    try {
        // Send to cloud with extra safety
        if (http_ && !cloud_base_url_.empty()) {
            resp = http_->post((cloud_base_url_ + "/api/inverter/fota/status").c_str(),
                               json_str.c_str(), json_str.length(), "application/json");
        } else {
            Logger::error("[FOTA] HTTP client or base URL not initialized");
            return false;
        }
    } catch (...) {
        Logger::error("[FOTA] Exception during HTTP POST");
        return false;
    }
    
    Logger::debug("[FOTA] HTTP POST completed - status: %d", resp.status_code);
    
    if (!resp.isSuccess()) {
        Logger::warn("[FOTA] Failed to report progress: status=%d", resp.status_code);
        return false;
    }
    
    return true;
}

bool FOTAManager::reportBootStatus() {
    Logger::info("[FOTA] Reporting boot status");
    
    int boot_count = getBootCount();
    bool boot_success = (boot_count == 0);
    
    DynamicJsonDocument doc(512);
    auto status = doc.createNestedObject("fota_status");
    
    if (boot_success) {
        status["boot_status"] = "success";
        status["new_version"] = getCurrentFirmwareVersion();
        clearBootCount();
        
        logFOTAEvent("boot_successful", "Version: " + getCurrentFirmwareVersion());
        Logger::info("[FOTA] Boot successful, firmware update completed");
    } else {
        status["boot_status"] = "failed";
        status["boot_count"] = boot_count;
        
        if (boot_count >= MAX_BOOT_ATTEMPTS) {
            status["rollback"] = true;
            logFOTAEvent("boot_failed_max_attempts", "Boot count: " + std::to_string(boot_count));
        } else {
            logFOTAEvent("boot_failed", "Boot count: " + std::to_string(boot_count));
        }
        
        Logger::warn("[FOTA] Boot count: %d", boot_count);
    }
    
    std::string json_str;
    serializeJson(doc, json_str);
    
    EcoHttpResponse resp = http_->post((cloud_base_url_ + "/api/inverter/fota/status").c_str(),
                                       json_str.c_str(), json_str.length(), "application/json");
    
    return resp.isSuccess();
}

void FOTAManager::cancel() {
    Logger::info("[FOTA] Cancelling FOTA operation");
    setState(FOTAState::IDLE, "Cancelled by user");
    clearFirmwareFile();
    reset();
}

void FOTAManager::reset() {
    progress_ = FOTAProgress();
    manifest_ = FOTAManifest();
    chunks_downloaded_.clear();
    
    // Remove state file
    if (LittleFS.exists(STATE_FILE)) {
        LittleFS.remove(STATE_FILE);
    }
}

void FOTAManager::loop() {
    // Only process if we're actually downloading
    if (progress_.state != FOTAState::DOWNLOADING) {
        return;
    }
    
    // Add significant throttling to prevent crashes
    static unsigned long last_chunk_time = 0;
    unsigned long now = millis();
    
    // Wait at least 10 seconds between chunk downloads
    const unsigned long CHUNK_INTERVAL = 10000;
    
    if (now - last_chunk_time < CHUNK_INTERVAL) {
        return; // Not time yet
    }
    
    // Safety check: ensure we have proper state
    if (manifest_.total_chunks == 0 || chunks_downloaded_.size() != manifest_.total_chunks) {
        Logger::error("[FOTA] Invalid manifest state in loop, stopping auto-processing");
        setState(FOTAState::FAILED, "Invalid manifest state");
        return;
    }
    
    // Process one chunk with extensive safety
    Logger::info("[FOTA] Auto-processing next chunk (10s interval)");
    bool success = processChunk();
    last_chunk_time = now;
    
    // Give the system time to breathe
    yield();
    delay(100);
    
    if (!success) {
        Logger::error("[FOTA] Chunk processing failed, stopping auto-processing");
        // Don't set to failed immediately, just stop auto-processing
        // Manual retry is still possible
    }
}

// ========== Private Helper Functions ==========

bool FOTAManager::fetchManifest() {
    Logger::info("[FOTA] Fetching manifest from cloud");
    
    EcoHttpResponse resp = http_->get((cloud_base_url_ + "/api/inverter/fota/manifest").c_str());
    
    if (!resp.isSuccess()) {
        Logger::error("[FOTA] Failed to fetch manifest: status=%d", resp.status_code);
        return false;
    }
    
    // Parse JSON response
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, resp.body.c_str());
    
    if (error) {
        Logger::error("[FOTA] Failed to parse manifest JSON: %s", error.c_str());
        return false;
    }
    
    // Check if FOTA available
    if (!doc.containsKey("fota")) {
        Logger::info("[FOTA] No FOTA manifest in response");
        manifest_.valid = false;
        return true; // Not an error, just no update
    }
    
    JsonObject fota = doc["fota"];
    if (!fota.containsKey("manifest")) {
        Logger::info("[FOTA] No manifest in FOTA response");
        manifest_.valid = false;
        return true;
    }
    
    JsonObject manifest_obj = fota["manifest"];
    
    // Extract manifest fields
    manifest_.version = manifest_obj["version"] | "";
    manifest_.size = manifest_obj["size"] | 0;
    manifest_.hash = manifest_obj["hash"] | "";
    manifest_.chunk_size = manifest_obj["chunk_size"] | 1024;
    
    // Guard against division by zero
    if (manifest_.size > 0 && manifest_.chunk_size > 0) {
        manifest_.total_chunks = (manifest_.size + manifest_.chunk_size - 1) / manifest_.chunk_size;
    } else {
        manifest_.total_chunks = 0;
        Logger::warn("[FOTA] Invalid manifest size or chunk_size: size=%u, chunk_size=%u", 
                    manifest_.size, manifest_.chunk_size);
    }
    
    manifest_.valid = !manifest_.version.empty() && manifest_.size > 0 && !manifest_.hash.empty();
    
    if (manifest_.valid) {
        Logger::info("[FOTA] Manifest loaded: version=%s, size=%u, chunks=%u, hash=%s",
                    manifest_.version.c_str(), manifest_.size, manifest_.total_chunks, 
                    manifest_.hash.substr(0, 16).c_str());
    }
    
    return true;
}

bool FOTAManager::fetchChunk(uint32_t chunk_number) {
    Logger::debug("[FOTA] Fetching chunk %u/%u", chunk_number + 1, manifest_.total_chunks);
    
    // Give other tasks a chance to run
    yield();
    delay(10);
    
    // Build request URL
    char url[256];
    snprintf(url, sizeof(url), "%s/api/inverter/fota/chunk?chunk_number=%u",
             cloud_base_url_.c_str(), chunk_number);
    
    EcoHttpResponse resp = http_->get(url);
    
    if (!resp.isSuccess()) {
        Logger::error("[FOTA] Failed to fetch chunk %u: status=%d", chunk_number, resp.status_code);
        return false;
    }
    
    // Give system time to process
    yield();
    
    // Parse JSON with memory check
    if (ESP.getFreeHeap() < 30000) {
        Logger::warn("[FOTA] Low memory before JSON parsing: %u bytes", ESP.getFreeHeap());
        delay(100); // Give time for cleanup
        yield();
    }
    
    DynamicJsonDocument doc(8192); // Large buffer for chunk data
    DeserializationError error = deserializeJson(doc, resp.body.c_str());
    
    if (error) {
        Logger::error("[FOTA] Failed to parse chunk JSON: %s", error.c_str());
        return false;
    }
    
    // Extract chunk data
    const char* data_b64 = doc["data"];
    const char* mac_hex = doc["mac"];
    uint32_t received_chunk_num = doc["chunk_number"] | 0xFFFFFFFF;
    
    if (!data_b64 || !mac_hex || received_chunk_num != chunk_number) {
        Logger::error("[FOTA] Invalid chunk response");
        return false;
    }
    
    // Decode base64
    size_t b64_len = strlen(data_b64);
    size_t decoded_len = (b64_len * 3) / 4;
    uint8_t* decoded = new uint8_t[decoded_len + 1];
    
    // Simple base64 decode (using Arduino's base64 or manual implementation)
    size_t actual_len = base64_decode((char*)decoded, data_b64, b64_len);
    
    if (actual_len == 0) {
        Logger::error("[FOTA] Failed to decode base64 chunk data");
        delete[] decoded;
        return false;
    }
    
    // Verify HMAC
    if (!verifyChunkHMAC(decoded, actual_len, std::string(mac_hex))) {
        Logger::error("[FOTA] Chunk %u HMAC verification failed", chunk_number);
        delete[] decoded;
        logFOTAEvent("chunk_hmac_failed", "Chunk: " + std::to_string(chunk_number));
        return false;
    }
    
    // Save chunk to file
    if (!saveFirmwareChunk(chunk_number, decoded, actual_len)) {
        Logger::error("[FOTA] Failed to save chunk %u", chunk_number);
        delete[] decoded;
        return false;
    }
    
    delete[] decoded;
    
    // Mark chunk as downloaded
    chunks_downloaded_[chunk_number] = true;
    progress_.chunks_received++;
    progress_.bytes_received += actual_len;
    
    {
        // Completely safe percentage calculation with multiple guards
        float pct = 0.0f;
        bool safe_to_calculate = false;
        
        // Multiple safety checks to prevent any division by zero scenario
        if (manifest_.total_chunks > 0 && 
            progress_.chunks_received <= manifest_.total_chunks &&
            progress_.chunks_received > 0) {
            
            // Additional runtime verification
            uint32_t total = manifest_.total_chunks;
            uint32_t received = progress_.chunks_received;
            
            if (total != 0 && received != 0 && received <= total) {
                pct = ((float)received * 100.0f) / (float)total;
                safe_to_calculate = true;
            }
        }
        
        if (safe_to_calculate) {
            Logger::info("[FOTA] Chunk %u downloaded and verified (%u/%u, %.1f%%)",
                        chunk_number, progress_.chunks_received, manifest_.total_chunks, pct);
        } else {
            // Fallback logging without percentage to avoid any calculation
            Logger::info("[FOTA] Chunk %u downloaded and verified (%u/%u)",
                        chunk_number, progress_.chunks_received, manifest_.total_chunks);
        }
    }
    
    // Save state periodically with safety checks
    if (progress_.chunks_received > 0 && manifest_.total_chunks > 0) {
        if (progress_.chunks_received % 5 == 0 || progress_.chunks_received == manifest_.total_chunks) {
            saveState();
        }
    }
    
    return true;
}

bool FOTAManager::saveState() {
    DynamicJsonDocument doc(2048);
    
    doc["state"] = (int)progress_.state;
    doc["version"] = manifest_.version;
    doc["chunks_received"] = progress_.chunks_received;
    doc["total_chunks"] = progress_.total_chunks;
    doc["verified"] = progress_.verified;
    
    // Save chunk bitmap
    JsonArray chunks = doc.createNestedArray("chunks");
    for (bool downloaded : chunks_downloaded_) {
        chunks.add(downloaded ? 1 : 0);
    }
    
    File file = LittleFS.open(STATE_FILE, "w");
    if (!file) {
        Logger::error("[FOTA] Failed to open state file for writing");
        return false;
    }
    
    serializeJson(doc, file);
    file.close();
    
    return true;
}

bool FOTAManager::loadState() {
    if (!LittleFS.exists(STATE_FILE)) {
        return false;
    }
    
    File file = LittleFS.open(STATE_FILE, "r");
    if (!file) {
        return false;
    }
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Logger::error("[FOTA] Failed to parse state file: %s", error.c_str());
        return false;
    }
    
    progress_.state = (FOTAState)(doc["state"] | 0);
    manifest_.version = doc["version"] | "";
    progress_.chunks_received = doc["chunks_received"] | 0;
    progress_.total_chunks = doc["total_chunks"] | 0;
    progress_.verified = doc["verified"] | false;
    
    // Restore chunk bitmap
    chunks_downloaded_.clear();
    if (doc.containsKey("chunks")) {
        JsonArray chunks = doc["chunks"];
        for (JsonVariant chunk : chunks) {
            chunks_downloaded_.push_back(chunk.as<int>() != 0);
        }
    }
    
    return true;
}

bool FOTAManager::saveFirmwareChunk(uint32_t chunk_number, const uint8_t* data, size_t size) {
    // Open file in append mode
    File file = LittleFS.open(FIRMWARE_FILE, chunk_number == 0 ? "w" : "a");
    if (!file) {
        Logger::error("[FOTA] Failed to open firmware file");
        return false;
    }
    
    size_t written = file.write(data, size);
    file.close();
    
    if (written != size) {
        Logger::error("[FOTA] Write error: wrote %u of %u bytes", written, size);
        return false;
    }
    
    return true;
}

bool FOTAManager::clearFirmwareFile() {
    if (LittleFS.exists(FIRMWARE_FILE)) {
        return LittleFS.remove(FIRMWARE_FILE);
    }
    return true;
}

void FOTAManager::setState(FOTAState state, const std::string& error) {
    progress_.state = state;
    progress_.error_message = error;
    
    if (!error.empty()) {
        Logger::error("[FOTA] State changed to %d: %s", (int)state, error.c_str());
    } else {
        Logger::info("[FOTA] State changed to %d", (int)state);
    }
}

bool FOTAManager::verifyChunkHMAC(const uint8_t* data, size_t size, const std::string& mac_hex) {
    if (!security_) {
        Logger::warn("[FOTA] Security layer not available, skipping HMAC verification");
        return true;
    }
    
    // Compute HMAC
    std::string data_str((char*)data, size);
    std::string computed_mac = security_->computeHMAC(data_str, security_->getConfig().psk);
    
    return computed_mac == mac_hex;
}

int FOTAManager::getBootCount() {
    if (!LittleFS.exists(BOOT_COUNT_FILE)) {
        return 0;
    }
    
    File file = LittleFS.open(BOOT_COUNT_FILE, "r");
    if (!file) {
        return 0;
    }
    
    String count_str = file.readStringUntil('\n');
    file.close();
    
    return count_str.toInt();
}

void FOTAManager::incrementBootCount() {
    int count = getBootCount();
    count++;
    
    File file = LittleFS.open(BOOT_COUNT_FILE, "w");
    if (file) {
        file.println(count);
        file.close();
    }
}

void FOTAManager::clearBootCount() {
    if (LittleFS.exists(BOOT_COUNT_FILE)) {
        LittleFS.remove(BOOT_COUNT_FILE);
    }
}

std::string FOTAManager::getCurrentFirmwareVersion() {
    // Read version from a version file or use build timestamp
    if (LittleFS.exists("/version.txt")) {
        File file = LittleFS.open("/version.txt", "r");
        if (file) {
            String version = file.readStringUntil('\n');
            file.close();
            return version.c_str();
        }
    }
    
    // Fallback: use compile date/time
    char version[32];
    snprintf(version, sizeof(version), "1.0.0-%s-%s", __DATE__, __TIME__);
    return std::string(version);
}

void FOTAManager::logFOTAEvent(const std::string& event_type, const std::string& details) {
    Logger::info("[FOTA EVENT] %s: %s", event_type.c_str(), details.c_str());
    
    // Optionally send to cloud as well
    // This could be integrated into reportProgress or sent separately
}

// ========== Base64 Decode Helper ==========
// Simple base64 decode implementation
size_t base64_decode(char* output, const char* input, size_t inputLen) {
    static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    size_t out_len = 0;
    uint32_t bits = 0;
    int bits_collected = 0;
    
    for (size_t i = 0; i < inputLen; i++) {
        char c = input[i];
        if (c == '=') break;
        
        const char* p = strchr(base64_table, c);
        if (!p) continue;
        
        bits = (bits << 6) | (p - base64_table);
        bits_collected += 6;
        
        if (bits_collected >= 8) {
            bits_collected -= 8;
            output[out_len++] = (bits >> bits_collected) & 0xFF;
            bits &= (1 << bits_collected) - 1;
        }
    }
    
    return out_len;
}
