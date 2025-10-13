#pragma once
#include "config_manager.hpp"
#include "http_client.hpp"
#include "config_update.hpp"

class EcoHttpClient;
#include "ticker_fallback.hpp"
#include <cstdint>
#include <functional>

class RemoteConfigHandler {
public:
    RemoteConfigHandler(ConfigManager* config, EcoHttpClient* http);
    ~RemoteConfigHandler();

    void begin(uint32_t interval_ms = 60000);
    void end();
    void loop();
    void checkForConfigUpdate();
    void sendConfigAck(const ConfigUpdateAck& ack);
    void onConfigUpdate(std::function<void()> callback);
    void onCommand(std::function<void(const char*)> callback);
    
    // Parse config update request from JSON
    bool parseConfigUpdateRequest(const char* json, ConfigUpdateRequest& request);
    
    // Generate acknowledgment JSON
    std::string generateAckJson(const ConfigUpdateAck& ack);

private:
    Ticker pollTicker_;
    uint32_t pollInterval_ = 60000;
    bool running_ = false;
    ConfigManager* config_ = nullptr;
    EcoHttpClient* http_ = nullptr;
    std::function<void(const char*)> onCommandCallback_ = nullptr;
    std::function<void()> onUpdateCallback_ = nullptr;
    void pollTask();
    static void pollTaskWrapper();
    static RemoteConfigHandler* instance_;
};
