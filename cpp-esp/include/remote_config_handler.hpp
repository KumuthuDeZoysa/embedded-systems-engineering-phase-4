#pragma once
#include "config_manager.hpp"
#include "http_client.hpp"

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
    void updateConfigFromCloud();
    void onConfigUpdate(std::function<void()> callback);
    void onCommand(std::function<void(const char*)> callback);

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
    // Error handling, command parsing, etc.
};
