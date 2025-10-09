#include <Arduino.h>
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
    updateConfigFromCloud();
}

void RemoteConfigHandler::onConfigUpdate(std::function<void()> callback) {
    onUpdateCallback_ = callback;
}

void RemoteConfigHandler::onCommand(std::function<void(const char*)> callback) {
    onCommandCallback_ = callback;
}

void RemoteConfigHandler::updateConfigFromCloud() {
    if (!http_ || !config_) return;
    
    EcoHttpResponse resp = http_->get(config_->getApiConfig().config_endpoint.c_str());
    if (resp.isSuccess()) {
        // Config is now hardcoded; skip updateFromJsonObject
        if (onUpdateCallback_) onUpdateCallback_();
            std::vector<std::string> commands;
            // TODO: Populate commands from cloud response (if needed)
            // For now, just demonstrate callback usage
            for (const auto& cmd_str : commands) {
                if (onCommandCallback_) onCommandCallback_(cmd_str.c_str());
            }
    }
}

void RemoteConfigHandler::pollTaskWrapper() {
    if (instance_) {
        instance_->pollTask();
    }
}
