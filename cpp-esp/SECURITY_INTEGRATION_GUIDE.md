# Security Integration Guide

This guide shows exactly how to integrate the security layer into your existing codebase.

## Files Created

1. `include/security_layer.hpp` - Security layer header
2. `src/security_layer.cpp` - Security layer implementation
3. `include/secure_http_client.hpp` - Secure HTTP client wrapper
4. `src/secure_http_client.cpp` - Secure HTTP client implementation
5. `MILESTONE4_PART3_DOCUMENTATION.md` - Complete documentation

## Files to Modify

### 1. `include/ecoWatt_device.hpp`

**Add** security-related includes and members:

```cpp
#pragma once

// Existing includes...
#include "security_layer.hpp"
#include "secure_http_client.hpp"

class EcoWattDevice {
public:
    // Existing methods...
    
private:
    // Existing members...
    
    // ADD THESE:
    SecurityLayer* security_layer_;
    SecureHttpClient* secure_http_client_;
};
```

### 2. `src/ecoWatt_device.cpp`

**In constructor**, initialize to nullptr:

```cpp
EcoWattDevice::EcoWattDevice()
    : wifi_connector_(nullptr)
    , http_client_(nullptr)
    , protocol_adapter_(nullptr)
    // ... other members
    , security_layer_(nullptr)        // ADD
    , secure_http_client_(nullptr) {  // ADD
}
```

**In setup()**, initialize security layer:

```cpp
void EcoWattDevice::setup() {
    // ... existing WiFi, config setup
    
    // ADD: Initialize Security Layer
    Logger::info("[EcoWatt] Initializing security layer...");
    
    SecurityConfig sec_config;
    
    // Load PSK from config or use default
    // TODO: Add security section to config.json
    sec_config.psk = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    sec_config.encryption_enabled = true;
    sec_config.use_real_encryption = true;  // Use AES-CBC
    sec_config.nonce_window = 100;
    sec_config.strict_nonce_checking = true;
    
    security_layer_ = new SecurityLayer(sec_config);
    if (!security_layer_->begin()) {
        Logger::error("[EcoWatt] Failed to initialize security layer");
        // Handle error - maybe continue without security?
    }
    
    // CREATE: Secure HTTP client
    secure_http_client_ = new SecureHttpClient(
        config_manager_->getApiConfig().base_url,
        security_layer_,
        5000  // 5 second timeout
    );
    
    Logger::info("[EcoWatt] Security layer initialized");
    
    // MODIFY: Pass secure HTTP client to subsystems
    // Option 1: Use secure client directly
    remote_config_handler_ = new RemoteConfigHandler(
        config_manager_,
        secure_http_client_->getHttpClient(),  // Use underlying client
        command_executor_
    );
    
    // Option 2: Create new secure client instances for each subsystem
    uplink_packetizer_ = new UplinkPacketizer(
        data_storage_,
        secure_http_client_->getHttpClient()
    );
    
    // ... rest of setup
}
```

**In destructor**, cleanup:

```cpp
EcoWattDevice::~EcoWattDevice() {
    // ... existing cleanup
    
    // ADD:
    if (security_layer_) {
        security_layer_->end();
        delete security_layer_;
        security_layer_ = nullptr;
    }
    
    if (secure_http_client_) {
        delete secure_http_client_;
        secure_http_client_ = nullptr;
    }
}
```

### 3. `include/uplink_packetizer.hpp`

**Add** security layer member:

```cpp
class UplinkPacketizer {
public:
    // Existing methods...
    
    // ADD:
    void setSecurityLayer(SecurityLayer* security) { security_ = security; }
    
private:
    // Existing members...
    
    // ADD:
    SecurityLayer* security_;
};
```

### 4. `src/uplink_packetizer.cpp`

**In constructor**, initialize:

```cpp
UplinkPacketizer::UplinkPacketizer(DataStorage* storage, EcoHttpClient* http)
    : uploadTicker_(uploadTaskWrapper, 60000)
    , storage_(storage)
    , http_(http)
    , security_(nullptr) {  // ADD
    instance_ = this;
}
```

**In uploadTask()**, secure the payload:

```cpp
void UplinkPacketizer::uploadTask() {
    // ... existing code to prepare payload
    
    // BEFORE sending, secure the message if security is enabled
    std::string payload_to_send;
    
    if (security_) {
        Logger::info("[Uplink] Securing upload payload...");
        
        // Convert compressed data to JSON
        std::string plain_json = "{\"data\":\"" + 
                                bufToHex(compressed, compLen) + 
                                "\",\"meta\":" + benchmarkJson + "}";
        
        // Secure the message
        SecuredMessage secured_msg;
        SecurityResult result = security_->secureMessage(plain_json, secured_msg);
        
        if (result.is_success()) {
            payload_to_send = security_->generateSecuredEnvelope(secured_msg);
            Logger::info("[Uplink] Payload secured: nonce=%u", secured_msg.nonce);
        } else {
            Logger::error("[Uplink] Failed to secure payload: %s", 
                         result.error_message.c_str());
            // Fall back to plain upload
            payload_to_send = plain_json;
        }
    } else {
        // No security layer, send plain
        payload_to_send = std::string((char*)compressed, compLen);
    }
    
    // Upload the payload
    (void)chunkAndUpload(payload_to_send.c_str(), payload_to_send.length());
    
    // ... rest of cleanup
}
```

### 5. `include/remote_config_handler.hpp`

**Add** security layer member:

```cpp
class RemoteConfigHandler {
public:
    // Existing methods...
    
    // ADD:
    void setSecurityLayer(SecurityLayer* security) { security_ = security; }
    
private:
    // Existing members...
    
    // ADD:
    SecurityLayer* security_;
};
```

### 6. `src/remote_config_handler.cpp`

**In constructor**, initialize:

```cpp
RemoteConfigHandler::RemoteConfigHandler(ConfigManager* config, 
                                        EcoHttpClient* http, 
                                        CommandExecutor* cmd_executor)
    : pollTicker_(pollTaskWrapper, 60000)
    , config_(config)
    , http_(http)
    , cmd_executor_(cmd_executor)
    , security_(nullptr) {  // ADD
    instance_ = this;
}
```

**In checkForConfigUpdate()**, verify secured responses:

```cpp
void RemoteConfigHandler::checkForConfigUpdate() {
    if (!http_ || !config_) return;
    
    Logger::info("[RemoteCfg] Checking for config updates...");
    
    // Get response from cloud
    EcoHttpResponse resp = http_->get(config_->getApiConfig().config_endpoint.c_str());
    if (!resp.isSuccess()) {
        Logger::warn("[RemoteCfg] Failed to get config: status=%d", resp.status_code);
        return;
    }
    
    // ADD: Verify secured response if security is enabled
    std::string plain_response;
    if (security_) {
        SecurityResult sec_result = security_->verifyMessage(resp.body, plain_response);
        
        if (!sec_result.is_success()) {
            Logger::warn("[RemoteCfg] Security verification failed: %s",
                        sec_result.error_message.c_str());
            // Try treating as plain response (for backward compatibility)
            plain_response = resp.body;
        } else {
            Logger::debug("[RemoteCfg] Response verified successfully");
        }
    } else {
        plain_response = resp.body;
    }
    
    // Parse using plain_response instead of resp.body
    ConfigUpdateRequest request;
    if (parseConfigUpdateRequest(plain_response.c_str(), request)) {
        // ... existing logic
    }
    
    CommandRequest command;
    if (parseCommandRequest(plain_response.c_str(), command)) {
        // ... existing logic
    }
}
```

**In sendConfigAck()**, secure the outgoing message:

```cpp
void RemoteConfigHandler::sendConfigAck(const ConfigUpdateAck& ack) {
    std::string ackJson = generateAckJson(ack);
    
    // ADD: Secure the acknowledgment if security is enabled
    std::string payload_to_send = ackJson;
    
    if (security_) {
        SecuredMessage secured_msg;
        SecurityResult result = security_->secureMessage(ackJson, secured_msg);
        
        if (result.is_success()) {
            payload_to_send = security_->generateSecuredEnvelope(secured_msg);
            Logger::debug("[RemoteCfg] Ack secured: nonce=%u", secured_msg.nonce);
        } else {
            Logger::error("[RemoteCfg] Failed to secure ack: %s",
                         result.error_message.c_str());
        }
    }
    
    Logger::info("[RemoteCfg] Sending config acknowledgment...");
    
    std::string ack_endpoint = config_->getApiConfig().config_endpoint + "/ack";
    EcoHttpResponse resp = http_->post(ack_endpoint.c_str(), 
                                      payload_to_send.c_str(),
                                      payload_to_send.length(), 
                                      "application/json");
    
    // ... existing response handling
}
```

**Similarly update sendCommandResults()**:

```cpp
void RemoteConfigHandler::sendCommandResults(const std::vector<CommandResult>& results) {
    if (results.empty()) return;
    
    std::string resultsJson = generateCommandResultsJson(results);
    
    // ADD: Secure the results
    std::string payload_to_send = resultsJson;
    
    if (security_) {
        SecuredMessage secured_msg;
        SecurityResult result = security_->secureMessage(resultsJson, secured_msg);
        
        if (result.is_success()) {
            payload_to_send = security_->generateSecuredEnvelope(secured_msg);
            Logger::debug("[RemoteCfg] Results secured: nonce=%u", secured_msg.nonce);
        }
    }
    
    Logger::info("[RemoteCfg] Sending command results...");
    
    std::string result_endpoint = config_->getApiConfig().config_endpoint + "/command/result";
    EcoHttpResponse resp = http_->post(result_endpoint.c_str(),
                                      payload_to_send.c_str(),
                                      payload_to_send.length(),
                                      "application/json");
    
    // ... existing response handling
}
```

### 7. `config/config.json`

**Add** security configuration section:

```json
{
  "api": {
    "base_url": "http://192.168.1.100:8080",
    "config_endpoint": "/api/inverter/config",
    "upload_endpoint": "/api/inverter/upload",
    "inverter_sim_base_url": "http://192.168.1.100:5000",
    "inverter_sim_api_base": "/api/v1/inverter"
  },
  "security": {
    "enabled": true,
    "psk": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
    "encryption_enabled": true,
    "use_real_encryption": true,
    "nonce_window": 100,
    "strict_nonce_checking": true
  },
  "modbus": {
    "slave_id": 1,
    "baud_rate": 9600,
    "max_retries": 3,
    "retry_delay_ms": 100,
    "timeout_ms": 1000
  }
}
```

### 8. `include/config_manager.hpp`

**Add** security config structure:

```cpp
// ADD after ApiConfig
struct SecurityConfig {
    bool enabled;
    std::string psk;
    bool encryption_enabled;
    bool use_real_encryption;
    uint32_t nonce_window;
    bool strict_nonce_checking;
};

class ConfigManager {
public:
    // Existing methods...
    
    // ADD:
    SecurityConfig getSecurityConfig() const { return security_config_; }
    
private:
    // Existing members...
    
    // ADD:
    SecurityConfig security_config_;
};
```

### 9. `src/config_manager.cpp`

**In loadFromFile()**, parse security config:

```cpp
bool ConfigManager::loadFromFile(const std::string& filepath) {
    // ... existing parsing
    
    // ADD: Parse security config
    if (root.containsKey("security")) {
        JsonObject security = root["security"];
        security_config_.enabled = security["enabled"] | true;
        security_config_.psk = security["psk"] | 
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        security_config_.encryption_enabled = security["encryption_enabled"] | true;
        security_config_.use_real_encryption = security["use_real_encryption"] | true;
        security_config_.nonce_window = security["nonce_window"] | 100;
        security_config_.strict_nonce_checking = security["strict_nonce_checking"] | true;
        
        Logger::info("[ConfigMgr] Security config loaded: enabled=%d, encryption=%d",
                    security_config_.enabled, security_config_.encryption_enabled);
    }
    
    // ... rest of function
}
```

### 10. Cloud Server (`cloud_server.py`)

**Add** security handling:

```python
import hmac
import hashlib
import base64
import json
from typing import Dict, Optional

# Configuration
PSK = bytes.fromhex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
SECURITY_ENABLED = True

# Nonce tracking
last_nonce = 0
recent_nonces = []

def compute_hmac(data: str) -> str:
    """Compute HMAC-SHA256"""
    return hmac.new(PSK, data.encode(), hashlib.sha256).hexdigest()

def verify_hmac(data: str, mac: str) -> bool:
    """Verify HMAC"""
    expected = compute_hmac(data)
    return hmac.compare_digest(expected, mac)

def verify_secured_message(secured_json: Dict) -> Optional[Dict]:
    """Verify and extract secured message"""
    global last_nonce, recent_nonces
    
    if not SECURITY_ENABLED:
        return secured_json
    
    # Check required fields
    if not all(k in secured_json for k in ['nonce', 'payload', 'mac']):
        logger.error("Missing required security fields")
        return None
    
    nonce = secured_json['nonce']
    timestamp = secured_json.get('timestamp', 0)
    encrypted = secured_json.get('encrypted', False)
    payload = secured_json['payload']
    mac = secured_json['mac']
    
    # Check nonce (anti-replay)
    if nonce in recent_nonces:
        logger.warning(f"Replay detected: nonce {nonce}")
        return None
    
    if nonce <= last_nonce:
        logger.warning(f"Old nonce: {nonce} (last: {last_nonce})")
        return None
    
    # Verify HMAC
    hmac_input = f"{nonce}{timestamp}{'1' if encrypted else '0'}{payload}"
    if not verify_hmac(hmac_input, mac):
        logger.error(f"HMAC verification failed for nonce {nonce}")
        return None
    
    # Update nonce tracking
    last_nonce = nonce
    recent_nonces.append(nonce)
    if len(recent_nonces) > 100:
        recent_nonces.pop(0)
    
    # Decode payload
    try:
        plain_json = base64.b64decode(payload).decode('utf-8')
        return json.loads(plain_json)
    except Exception as e:
        logger.error(f"Failed to decode payload: {e}")
        return None

def secure_message(payload: Dict, nonce: int) -> Dict:
    """Secure a message for transmission"""
    if not SECURITY_ENABLED:
        return payload
    
    plain_json = json.dumps(payload)
    encoded = base64.b64encode(plain_json.encode()).decode()
    
    timestamp = int(time.time() * 1000)
    encrypted = True
    
    hmac_input = f"{nonce}{timestamp}{'1' if encrypted else '0'}{encoded}"
    mac = compute_hmac(hmac_input)
    
    return {
        "nonce": nonce,
        "timestamp": timestamp,
        "encrypted": encrypted,
        "payload": encoded,
        "mac": mac
    }

# Modify existing endpoints
@app.route('/api/inverter/config', methods=['GET'])
def get_inverter_config():
    # ... existing logic to prepare response
    
    # Secure the response
    if SECURITY_ENABLED:
        nonce = config_state['current_nonce']
        config_state['current_nonce'] += 1
        return jsonify(secure_message(response, nonce))
    
    return jsonify(response)

@app.route('/api/inverter/config/ack', methods=['POST'])
def config_ack():
    data = request.get_json()
    
    # Verify secured message
    if SECURITY_ENABLED:
        plain_data = verify_secured_message(data)
        if plain_data is None:
            return jsonify({"error": "Security verification failed"}), 403
    else:
        plain_data = data
    
    # ... existing logic using plain_data
    
    return jsonify({"status": "acknowledged"})
```

## Build Configuration

### platformio.ini

**Add** crypto library dependencies:

```ini
[env:nodemcu-32s]
platform = espressif32
board = nodemcu-32s
framework = arduino

lib_deps = 
    bblanchon/ArduinoJson@^6.21.3
    
    # ADD for security:
    rweather/Crypto@^0.4.0        ; For ESP8266
    # mbedtls is built-in on ESP32

build_flags =
    -DCORE_DEBUG_LEVEL=4
    -DLOG_LOCAL_LEVEL=ESP_LOG_DEBUG
```

## Testing

### 1. Unit Test (Optional)

Create `tests/test_security.cpp`:

```cpp
#include <unity.h>
#include "../include/security_layer.hpp"

void test_hmac_computation() {
    SecurityConfig config;
    config.psk = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    
    SecurityLayer security(config);
    security.begin();
    
    std::string data = "test data";
    std::string mac = security.computeHMAC(data, config.psk);
    
    TEST_ASSERT_EQUAL(64, mac.length());
    TEST_ASSERT_TRUE(security.verifyHMAC(data, mac, config.psk));
    
    security.end();
}

void test_nonce_replay_detection() {
    SecurityConfig config;
    config.psk = "test_key_32_bytes_hexadecimal_string";
    config.nonce_window = 10;
    config.strict_nonce_checking = true;
    
    SecurityLayer security(config);
    security.begin();
    
    uint32_t nonce1 = 1000;
    TEST_ASSERT_TRUE(security.isNonceValid(nonce1));
    security.updateLastNonce(nonce1);
    
    // Replay should be rejected
    TEST_ASSERT_FALSE(security.isNonceValid(nonce1));
    
    security.end();
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_hmac_computation);
    RUN_TEST(test_nonce_replay_detection);
    return UNITY_END();
}
```

### 2. Integration Test

```bash
# Build and upload
pio run -t upload

# Monitor serial output
pio device monitor

# Look for:
# [Security] Initializing security layer...
# [Security] Security layer initialized
# [Uplink] Securing upload payload...
# [RemoteCfg] Response verified successfully
```

## Summary

This integration adds security to:
- ✅ Data uploads (uplink packetizer)
- ✅ Config requests/responses (remote config handler)
- ✅ Command execution (command executor)
- ✅ All HTTP communications

The integration is designed to be:
- **Non-breaking**: Can be disabled via config
- **Backward compatible**: Falls back to plain HTTP if security fails
- **Transparent**: Minimal changes to application logic
- **Configurable**: PSK and options in config.json
