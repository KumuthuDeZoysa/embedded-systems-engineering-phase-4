#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <cstring>
#include <cstdlib>
#include <string>
#include <functional>
#include "ticker_fallback.hpp"
#include "../include/ecoWatt_device.hpp"
#include "../include/acquisition_scheduler.hpp"
#include "../include/data_storage.hpp"
#include "../include/protocol_adapter.hpp"
#include "../include/config_manager.hpp"
#include "../include/uplink_packetizer.hpp"
#include "../include/remote_config_handler.hpp"
#include "../include/http_client.hpp"
#include "../include/logger.hpp"
#include "../include/wifi_connector.hpp"
// --- Heap/Stack debug print helper ---
#ifdef ESP32
#include <Arduino.h>
#endif
#include <Arduino.h>
static void printMemoryStats(const char* tag) {
#ifdef ESP32
    Logger::info("[MEM] %s | Free heap: %u bytes | Min heap: %u bytes", tag, ESP.getFreeHeap(), ESP.getMinFreeHeap());
#endif
    char dummy;
    Logger::info("[MEM] %s | Stack ptr: %p", tag, &dummy);
}
EcoWattDevice::EcoWattDevice() {}
EcoWattDevice::~EcoWattDevice() {
    delete scheduler_;
    delete adapter_;
    delete storage_;
    delete uplink_packetizer_;
    delete remote_config_handler_;
    delete http_client_;
    delete config_;
    delete wifi_;
}

// Simple JSON helpers (very small, only for our known command shapes)
static bool extractJsonStringFieldLocal(const char* json, const char* key, char* outBuf, size_t outBufSize) {
    if (!json || !key || !outBuf) return false;
    char pat[128];
    size_t klen = strlen(key);
    if (klen + 3 >= sizeof(pat)) return false;
    pat[0] = '"';
    memcpy(pat + 1, key, klen);
    pat[1 + klen] = '"';
    pat[2 + klen] = '\0';
    const char* found = strstr(json, pat);
    if (!found) return false;
    const char* colon = strchr(found + strlen(pat), ':');
    if (!colon) return false;
    const char* q1 = strchr(colon, '"');
    if (!q1) return false;
    const char* q2 = strchr(q1 + 1, '"');
    if (!q2) return false;
    size_t len = q2 - (q1 + 1);
    if (len + 1 > outBufSize) return false;
    memcpy(outBuf, q1 + 1, len);
    outBuf[len] = '\0';
    return true;
}

static bool extractJsonNumberFieldLocal(const char* json, const char* key, char* outNumBuf, size_t outNumBufSize) {
    if (!json || !key || !outNumBuf) return false;
    char pat[128];
    size_t klen = strlen(key);
    if (klen + 2 >= sizeof(pat)) return false;
    pat[0] = '"';
    memcpy(pat + 1, key, klen);
    pat[1 + klen] = '"';
    pat[2 + klen] = '\0';
    const char* found = strstr(json, pat);
    if (!found) return false;
    const char* colon = strchr(found + strlen(pat), ':');
    if (!colon) return false;
    // Skip spaces
    const char* p = colon + 1;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    // Read until non-number (allow digits, minus, dot, exponent, etc.)
    const char* start = p;
    const char* end = start;
    while (*end && ( (*end >= '0' && *end <= '9') || *end == '-' || *end == '+' || *end == '.' || *end == 'e' || *end == 'E')) end++;
    size_t len = end - start;
    if (len == 0 || len + 1 > outNumBufSize) return false;
    memcpy(outNumBuf, start, len);
    outNumBuf[len] = '\0';
    return true;
}

bool EcoWattDevice::isOnline() const {
    return WiFi.status() == WL_CONNECTED;
}

float EcoWattDevice::getReading(uint8_t reg_addr) const {
    if (!adapter_ || !config_) return 0.0f;
    uint16_t raw_value = 0;
    if (adapter_->readRegisters(reg_addr, 1, &raw_value)) {
        RegisterConfig reg_config = config_->getRegisterConfig(reg_addr);
        if (reg_config.gain > 0) {
            return (float)raw_value / reg_config.gain;
        }
        return (float)raw_value;
    }
    return 0.0f; // Or NAN
}

bool EcoWattDevice::setControl(uint8_t reg_addr, float value) {
    if (!adapter_ || !config_) return false;

    RegisterConfig reg_config = config_->getRegisterConfig(reg_addr);
    if (reg_config.access.find("Write") == std::string::npos) {
        return false; // Not a writable register
    }

    uint16_t raw_value = (uint16_t)(value * reg_config.gain);
    return adapter_->writeRegister(reg_addr, raw_value);
}

void EcoWattDevice::getStatistics(char* outBuf, size_t outBufSize) const {
    // This can be expanded to gather more detailed stats from other modules
    // For example: scheduler_->getStatistics(buf, size);
    // For now, we provide basic uptime and connectivity.
    snprintf(outBuf, outBufSize, "uptime=%lu, online=%d", millis(), isOnline());
}

void EcoWattDevice::onConfigUpdated() {
    Logger::info("Remote configuration updated. Applying changes...");
    AcquisitionConfig acq_conf = config_->getAcquisitionConfig();
    scheduler_->updateConfig(acq_conf.minimum_registers.data(), acq_conf.minimum_registers.size(), acq_conf.polling_interval_ms);
}

void EcoWattDevice::executeCommand(const char* cmdJson) {
    if (!adapter_ || !cmdJson) return;

    char commandBuf[64];
    if (!extractJsonStringFieldLocal(cmdJson, "command", commandBuf, sizeof(commandBuf))) {
        Logger::warn("Command JSON missing or invalid 'command' field");
        return;
    }

    if (strcmp(commandBuf, "write") == 0) {
        char regBuf[32];
        char valBuf[64];
        if (!extractJsonNumberFieldLocal(cmdJson, "register", regBuf, sizeof(regBuf))) {
            Logger::warn("Command JSON missing or invalid 'register' field");
            return;
        }
        if (!extractJsonNumberFieldLocal(cmdJson, "value", valBuf, sizeof(valBuf))) {
            Logger::warn("Command JSON missing or invalid 'value' field");
            return;
        }
        int reg_addr = atoi(regBuf);
        float value = strtof(valBuf, NULL);

        if (config_) {
            RegisterConfig reg_config = config_->getRegisterConfig((uint8_t)reg_addr);
            uint16_t raw_value = 0;
            if (reg_config.gain > 0) {
                raw_value = (uint16_t)(value * reg_config.gain);
            } else {
                raw_value = (uint16_t)(value);
            }
            adapter_->writeRegister((uint8_t)reg_addr, raw_value);
        }
    }
}

void EcoWattDevice::setup() {
    Logger::info("EcoWatt Device initializing...");
    
    // Initialize WiFi, SPIFFS, config, etc.
    if (!config_) {
        config_ = new ConfigManager();
        Logger::info("ConfigManager initialized");
    }
    if (!storage_) {
        storage_ = new DataStorage();
        Logger::info("DataStorage initialized");
    }

    ApiConfig api_conf = config_->getApiConfig();
    ModbusConfig mbc = config_->getModbusConfig();
    if (!http_client_) {
        http_client_ = new EcoHttpClient(api_conf.inverter_base_url, mbc.timeout_ms);
        Logger::info("HTTP Client initialized with base URL: %s", api_conf.inverter_base_url.c_str());
    }

    // WiFi: use wifi config if present in config.json, otherwise rely on /config/.env overrides
    if (!wifi_) {
        wifi_ = new WiFiConnector();
        Logger::info("WiFi Connector initialized");
    }
    // Hardcoded WiFi credentials
    wifi_->begin();
    
    // Wait for WiFi connection
    Logger::info("Waiting for WiFi connection...");
    unsigned long wifiStart = millis();
    while (!wifi_->isConnected() && millis() - wifiStart < 30000) { // 30 second timeout
        wifi_->loop();
        delay(500);
    }
    if (wifi_->isConnected()) {
        Logger::info("WiFi connected successfully");
    } else {
        Logger::error("WiFi connection failed after 30 seconds");
    }
    
    // Set the mandatory API key for all requests
    const char* header_keys[] = {"Authorization"};
    const char* header_values[] = {api_conf.api_key.c_str()};
    http_client_->setDefaultHeaders(header_keys, header_values, 1);
    Logger::info("API key configured for requests");

    if (!adapter_) {
        adapter_ = new ProtocolAdapter(config_, http_client_);
        Logger::info("ProtocolAdapter initialized with slave address %d", mbc.slave_address);
    }

    if (!uplink_packetizer_) {
        uplink_packetizer_ = new UplinkPacketizer(storage_, http_client_);
        // Use the upload endpoint directly (should be a full URL)
        uplink_packetizer_->setCloudEndpoint(api_conf.upload_endpoint);
        uplink_packetizer_->begin(15 * 1000); // Upload every 15 seconds for demo
        Logger::info("UplinkPacketizer initialized, upload interval: 15 seconds (demo mode)");
    }

    if (!scheduler_) {
        scheduler_ = new AcquisitionScheduler(adapter_, storage_, config_);
        AcquisitionConfig acq_conf = config_->getAcquisitionConfig();
        scheduler_->updateConfig(acq_conf.minimum_registers.data(), acq_conf.minimum_registers.size(), acq_conf.polling_interval_ms);
        scheduler_->begin(acq_conf.polling_interval_ms);
        Logger::info("AcquisitionScheduler initialized with polling interval: %d ms", acq_conf.polling_interval_ms);
    }

    if (!remote_config_handler_) {
        remote_config_handler_ = new RemoteConfigHandler(config_, http_client_);
        using namespace std::placeholders;
        remote_config_handler_->onConfigUpdate([this]() { onConfigUpdated(); });
        remote_config_handler_->onCommand(std::bind(&EcoWattDevice::executeCommand, this, _1));
        remote_config_handler_->begin(60000); // Check for new config every 60 seconds
        Logger::info("RemoteConfigHandler initialized, check interval: 60 seconds");
    }

    // Perform a one-time write operation as part of Milestone 2 requirements.
    // Write a safe default to a writable register if configured.
    // If register 8 is present and writable, write zero (as a demo)
    RegisterConfig rc = config_->getRegisterConfig(8);
    if (rc.access.find("Write") != std::string::npos) {
        uint16_t raw_value = (uint16_t)(0 * rc.gain);
        bool ok = adapter_->writeRegister(8, raw_value);
        Logger::info("Demo write to reg 8 result: %d", ok);
    }
    
    Logger::info("EcoWatt Device initialized successfully");
}

void EcoWattDevice::loop() {
    // Main polling, control, and data acquisition loop
    printMemoryStats("MainLoop");
    if (storage_) storage_->loop();
    if (scheduler_) scheduler_->loop();
    if (uplink_packetizer_) uplink_packetizer_->loop();
    if (remote_config_handler_) remote_config_handler_->loop();
    if (wifi_) wifi_->loop();
    // Other device logic...
}
