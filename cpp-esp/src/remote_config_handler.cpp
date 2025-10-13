#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <string>
#include "../include/ticker_fallback.hpp"
#include "../include/remote_config_handler.hpp"
#include "../include/logger.hpp"

RemoteConfigHandler* RemoteConfigHandler::instance_ = nullptr;

RemoteConfigHandler::RemoteConfigHandler(ConfigManager* config, EcoHttpClient* http)
    : pollTicker_(pollTaskWrapper, 60000), config_(config), http_(http) {
    instance_ = this;
}
RemoteConfigHandler::~RemoteConfigHandler() { end(); }

void RemoteConfigHandler::begin(uint32_t interval_ms) {
    pollInterval_ = interval_ms;
    pollTicker_.interval(pollInterval_);
    running_ = true;
    pollTicker_.start();
}

void RemoteConfigHandler::end() {
    pollTicker_.stop();
    running_ = false;
}

void RemoteConfigHandler::loop() {
    if (running_) {
        pollTicker_.update();
    }
}

void RemoteConfigHandler::pollTask() {
    checkForConfigUpdate();
}

void RemoteConfigHandler::onConfigUpdate(std::function<void()> callback) {
    onUpdateCallback_ = callback;
}

void RemoteConfigHandler::onCommand(std::function<void(const char*)> callback) {
    onCommandCallback_ = callback;
}

void RemoteConfigHandler::checkForConfigUpdate() {
    if (!http_ || !config_) return;
    
    Logger::info("[RemoteCfg] Checking for config updates from cloud...");
    
    // Request config update from cloud
    EcoHttpResponse resp = http_->get(config_->getApiConfig().config_endpoint.c_str());
    if (!resp.isSuccess()) {
        Logger::warn("[RemoteCfg] Failed to get config from cloud: status=%d", resp.status_code);
        return;
    }
    
    // Parse the response
    ConfigUpdateRequest request;
    if (!parseConfigUpdateRequest(resp.body.c_str(), request)) {
        Logger::warn("[RemoteCfg] Failed to parse config update request");
        return;
    }
    
    // Apply the configuration update
    ConfigUpdateAck ack = config_->applyConfigUpdate(request);
    
    // Send acknowledgment back to cloud
    sendConfigAck(ack);
    
    // Trigger callback if any parameters were accepted
    if (!ack.accepted.empty() && onUpdateCallback_) {
        onUpdateCallback_();
    }
    
    // Handle commands (if any in response)
    // TODO: Implement command handling in Part 2
}

bool RemoteConfigHandler::parseConfigUpdateRequest(const char* json, ConfigUpdateRequest& request) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Logger::error("[RemoteCfg] JSON parse error: %s", error.c_str());
        return false;
    }
    
    // Initialize request
    request.has_sampling_interval = false;
    request.has_registers = false;
    request.nonce = 0;
    request.timestamp = millis();
    
    // Check if there's a config_update object
    if (!doc.containsKey("config_update")) {
        Logger::debug("[RemoteCfg] No config_update in response");
        return false;
    }
    
    JsonObject config_update = doc["config_update"];
    
    // Parse nonce (if present)
    if (doc.containsKey("nonce")) {
        request.nonce = doc["nonce"].as<uint32_t>();
    } else {
        // Generate nonce from timestamp if not provided
        request.nonce = request.timestamp;
    }
    
    // Parse sampling interval
    if (config_update.containsKey("sampling_interval")) {
        // Convert from seconds to milliseconds (as per spec)
        uint32_t interval_seconds = config_update["sampling_interval"].as<uint32_t>();
        request.sampling_interval_ms = interval_seconds * 1000;
        request.has_sampling_interval = true;
        Logger::debug("[RemoteCfg] Parsed sampling_interval: %u ms", request.sampling_interval_ms);
    }
    
    // Parse registers
    if (config_update.containsKey("registers")) {
        JsonArray registers = config_update["registers"];
        request.registers.clear();
        
        for (JsonVariant reg : registers) {
            // Support both numeric and string register names
            if (reg.is<int>()) {
                request.registers.push_back(reg.as<uint8_t>());
            } else if (reg.is<const char*>()) {
                // Map register name to address
                std::string name = reg.as<const char*>();
                // Simple mapping (you can expand this)
                if (name == "voltage") request.registers.push_back(0);
                else if (name == "current") request.registers.push_back(1);
                else if (name == "frequency") request.registers.push_back(2);
                else if (name == "pv1_voltage") request.registers.push_back(3);
                else if (name == "pv2_voltage") request.registers.push_back(4);
                else if (name == "pv1_current") request.registers.push_back(5);
                else if (name == "pv2_current") request.registers.push_back(6);
                else if (name == "temperature") request.registers.push_back(7);
                else if (name == "export_power") request.registers.push_back(8);
                else if (name == "output_power") request.registers.push_back(9);
                else {
                    Logger::warn("[RemoteCfg] Unknown register name: %s", name.c_str());
                }
            }
        }
        
        if (!request.registers.empty()) {
            request.has_registers = true;
            Logger::debug("[RemoteCfg] Parsed %u registers", (unsigned)request.registers.size());
        }
    }
    
    return request.has_sampling_interval || request.has_registers;
}

void RemoteConfigHandler::sendConfigAck(const ConfigUpdateAck& ack) {
    std::string ackJson = generateAckJson(ack);
    
    Logger::info("[RemoteCfg] Sending config acknowledgment to cloud");
    Logger::debug("[RemoteCfg] Ack JSON: %s", ackJson.c_str());
    
    // Send ACK to cloud (use config_endpoint with /ack suffix or dedicated endpoint)
    std::string ack_endpoint = config_->getApiConfig().config_endpoint + "/ack";
    EcoHttpResponse resp = http_->post(ack_endpoint.c_str(), ackJson.c_str(), 
                                       ackJson.length(), "application/json");
    
    if (resp.isSuccess()) {
        Logger::info("[RemoteCfg] Config acknowledgment sent successfully");
    } else {
        Logger::warn("[RemoteCfg] Failed to send config acknowledgment: status=%d", resp.status_code);
    }
}

std::string RemoteConfigHandler::generateAckJson(const ConfigUpdateAck& ack) {
    StaticJsonDocument<2048> doc;
    
    doc["nonce"] = ack.nonce;
    doc["timestamp"] = ack.timestamp;
    doc["all_success"] = ack.all_success;
    
    // Create config_ack object
    JsonObject config_ack = doc.createNestedObject("config_ack");
    
    // Accepted parameters
    JsonArray accepted = config_ack.createNestedArray("accepted");
    for (const auto& param : ack.accepted) {
        JsonObject p = accepted.createNestedObject();
        p["parameter"] = param.parameter_name;
        p["old_value"] = param.old_value;
        p["new_value"] = param.new_value;
        p["reason"] = param.reason;
    }
    
    // Rejected parameters
    JsonArray rejected = config_ack.createNestedArray("rejected");
    for (const auto& param : ack.rejected) {
        JsonObject p = rejected.createNestedObject();
        p["parameter"] = param.parameter_name;
        p["old_value"] = param.old_value;
        p["new_value"] = param.new_value;
        p["reason"] = param.reason;
    }
    
    // Unchanged parameters
    JsonArray unchanged = config_ack.createNestedArray("unchanged");
    for (const auto& param : ack.unchanged) {
        JsonObject p = unchanged.createNestedObject();
        p["parameter"] = param.parameter_name;
        p["reason"] = param.reason;
    }
    
    // Serialize to string
    std::string result;
    serializeJson(doc, result);
    return result;
}

void RemoteConfigHandler::pollTaskWrapper() {
    if (instance_) {
        instance_->pollTask();
    }
}
