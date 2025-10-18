# 🔧 Phase 2: Security Integration Changes

This file contains all the code changes needed to integrate security into the main application.

---

## File 1: include/ecoWatt_device.hpp

### ADD these includes at the top:
```cpp
#include "security_layer.hpp"
#include "secure_http_client.hpp"
```

### ADD these private members:
```cpp
private:
    // ... existing members ...
    SecurityLayer* security_ = nullptr;           // ADD THIS
    SecureHttpClient* secure_http_ = nullptr;     // ADD THIS
```

---

## File 2: src/ecoWatt_device.cpp

### IN setup() method, AFTER config initialization, ADD:

```cpp
// Initialize Security Layer (if enabled in config)
if (config_->security.enabled) {
    Logger::info("Initializing security layer...");
    
    SecurityConfig sec_config;
    sec_config.psk = config_->security.psk;
    sec_config.encryption_enabled = false;  // Using Base64 for now
    sec_config.use_real_encryption = config_->security.use_real_encryption;
    sec_config.strict_nonce_checking = config_->security.strict_nonce_checking;
    sec_config.nonce_window = config_->security.nonce_window;
    
    security_ = new SecurityLayer(sec_config);
    if (!security_->begin()) {
        Logger::error("Failed to initialize security layer!");
    } else {
        Logger::info("SecurityLayer initialized successfully");
    }
    
    // Create SecureHttpClient
    secure_http_ = new SecureHttpClient(config_->cloud.url, security_);
    Logger::info("SecureHttpClient initialized");
} else {
    Logger::warn("Security disabled in configuration");
}
```

### UPDATE RemoteConfigHandler initialization:
```cpp
// OLD:
remote_config_handler_ = new RemoteConfigHandler(config_, http_client_, command_executor_);

// NEW:
if (security_ && secure_http_) {
    remote_config_handler_ = new RemoteConfigHandler(config_, secure_http_, command_executor_);
} else {
    remote_config_handler_ = new RemoteConfigHandler(config_, http_client_, command_executor_);
}
```

### UPDATE UplinkPacketizer initialization:
```cpp
// OLD:
uplink_packetizer_ = new UplinkPacketizer(config_, http_client_);

// NEW:
if (security_ && secure_http_) {
    uplink_packetizer_ = new UplinkPacketizer(config_, secure_http_->getHttpClient());
} else {
    uplink_packetizer_ = new UplinkPacketizer(config_, http_client_);
}
```

### IN destructor, ADD cleanup:
```cpp
if (security_) {
    security_->end();
    delete security_;
    security_ = nullptr;
}
if (secure_http_) {
    delete secure_http_;
    secure_http_ = nullptr;
}
```

---

## File 3: include/remote_config_handler.hpp

### UPDATE constructor signature:
```cpp
// ADD this overload:
RemoteConfigHandler(ConfigManager* config, SecureHttpClient* secure_http, CommandExecutor* cmd_executor = nullptr);
```

### ADD private member:
```cpp
private:
    SecureHttpClient* secure_http_ = nullptr;  // ADD THIS
    bool use_security_ = false;                 // ADD THIS
```

---

## File 4: src/remote_config_handler.cpp

### ADD new constructor:
```cpp
RemoteConfigHandler::RemoteConfigHandler(ConfigManager* config, SecureHttpClient* secure_http, CommandExecutor* cmd_executor)
    : config_(config)
    , secure_http_(secure_http)
    , cmd_executor_(cmd_executor)
    , use_security_(true) {
    instance_ = this;
    Logger::info("RemoteConfigHandler initialized with security");
}
```

### UPDATE checkForConfigUpdate() method:
```cpp
void RemoteConfigHandler::checkForConfigUpdate() {
    if (!config_) return;
    
    Logger::info("[RemoteCfg] Checking for config updates and commands from cloud...");
    
    std::string plain_response;
    EcoHttpResponse response;
    
    if (use_security_ && secure_http_) {
        // Use secure GET
        response = secure_http_->secureGet("/api/inverter/config", plain_response);
    } else {
        // Use plain HTTP
        response = http_->get("/api/inverter/config");
        plain_response = response.body;
    }
    
    if (!response.isSuccess()) {
        Logger::warn("[RemoteCfg] Failed to get config from cloud: status=%d", response.status_code);
        return;
    }
    
    // Rest of the method uses plain_response instead of response.body
}
```

### UPDATE sendConfigAck() method:
```cpp
void RemoteConfigHandler::sendConfigAck(const ConfigUpdateAck& ack) {
    if (!http_ && !secure_http_) return;
    
    std::string ack_json = generateAckJson(ack);
    
    if (use_security_ && secure_http_) {
        // Use secure POST
        std::string plain_response;
        EcoHttpResponse response = secure_http_->securePost("/api/config/status", ack_json, plain_response);
        
        if (response.isSuccess()) {
            Logger::info("[RemoteCfg] Config acknowledgment sent successfully");
        } else {
            Logger::warn("[RemoteCfg] Failed to send config ack: status=%d", response.status_code);
        }
    } else {
        // Use plain HTTP
        EcoHttpResponse response = http_->post("/api/config/status", ack_json.c_str(), ack_json.length());
        
        if (response.isSuccess()) {
            Logger::info("[RemoteCfg] Config acknowledgment sent successfully");
        } else {
            Logger::warn("[RemoteCfg] Failed to send config ack: status=%d", response.status_code);
        }
    }
}
```

---

## Summary of Changes

1. **ecoWatt_device.hpp**: Add security includes and members
2. **ecoWatt_device.cpp**: Initialize security layer and secure HTTP client
3. **remote_config_handler.hpp**: Add secure HTTP constructor
4. **remote_config_handler.cpp**: Use secure HTTP when available

These changes maintain backward compatibility - if security is disabled, the code falls back to plain HTTP.

---

## Testing After Integration

After applying these changes:

1. Compile: `pio run`
2. Upload: `pio run -t upload -t monitor`
3. Check logs for: "SecurityLayer initialized successfully"
4. Verify: 401 errors should become 200 OK
5. Check server logs for HMAC verification

---

*This integration maintains backward compatibility and gracefully handles both secured and unsecured modes.*
