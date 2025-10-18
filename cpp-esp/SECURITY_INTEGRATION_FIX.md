# Security Integration Fix - Analysis & Solution

## Problem Analysis

### Current Issues

#### 1. **Remote Config Getting 401 Unauthorized** ❌
```
[WARN] [RemoteCfg] Failed to get config from cloud: status=401
```

**Root Cause:**
- ESP32 is sending **plain HTTP requests** to Flask backend
- Flask backend (`app.py`) has **SECURITY_ENABLED = True** and expects secured messages
- ESP32's `RemoteConfigHandler` uses regular `EcoHttpClient`, not `SecureHttpClient`
- No security integration between ESP32 ↔ Flask ↔ Node-RED

#### 2. **Upload/Uplink Failures** ❌
```
[WARN] UplinkPacketizer: failed to upload chunk at offset 0 after 3 attempts
[INFO] [Uplink] Benchmark metadata upload status: -1
```

**Root Cause:**
- Flask `/api/upload` endpoint expects secured messages
- ESP32's `UplinkPacketizer` uses regular `EcoHttpClient`
- Compressed binary data is not being wrapped in secured envelopes

## Architecture Overview

### Current System
```
┌─────────────┐         ┌─────────────┐         ┌─────────────┐
│   ESP32     │   HTTP  │    Flask    │   HTTP  │  Node-RED   │
│             │ ──────> │   (app.py)  │ ──────> │  Dashboard  │
│ - Remote    │  Plain  │             │         │             │
│   Config    │ ❌ 401  │ Security    │         │ - /dashboard│
│ - Uplink    │         │ Enforced    │         │ - /config   │
│             │         │             │         │ - /commands │
└─────────────┘         └─────────────┘         └─────────────┘
```

### Required System
```
┌─────────────┐         ┌─────────────┐         ┌─────────────┐
│   ESP32     │ Secured │    Flask    │   HTTP  │  Node-RED   │
│             │ ──────> │   (app.py)  │ ──────> │  Dashboard  │
│ - Secure    │  HMAC   │             │         │             │
│   HTTP      │  Nonce  │ ✅ Verify   │         │ - /dashboard│
│ - Security  │  MAC    │ ✅ Accept   │         │ - /config   │
│   Layer     │         │             │         │ - /commands │
└─────────────┘         └─────────────┘         └─────────────┘
```

## Detailed Analysis

### ESP32 Side (C++)

#### Files Requiring Changes:

### 1. **`src/remote_config_handler.cpp`** 🔴 CRITICAL
**Current Code (Line 53):**
```cpp
EcoHttpResponse resp = http_->get(config_->getApiConfig().config_endpoint.c_str());
```

**Problem:** Using plain `EcoHttpClient`, no security

**What needs to change:**
- Constructor should accept `SecureHttpClient*` instead of `EcoHttpClient*`
- Use `secure_http_->secureGet()` instead of `http_->get()`
- All HTTP operations need to be secured

**Affected Endpoints:**
- `GET /api/inverter/config` (Line 53)
- `POST /api/inverter/config/ack` (Line 194)
- `POST /api/inverter/config/command/result` (Line 304)

---

### 2. **`src/uplink_packetizer.cpp`** 🔴 CRITICAL
**Current Code:**
```cpp
EcoHttpResponse resp = http_->post(endpoint, payload, payload_size);
```

**Problem:** Using plain HTTP POST for compressed data upload

**What needs to change:**
- Constructor should accept `SecureHttpClient*`
- Wrap compressed binary data in secured envelope before sending
- Use `secure_http_->securePost()`

**Affected Endpoints:**
- `POST /api/upload` (binary data upload)
- `POST /api/upload/benchmark` (metadata)

---

### 3. **`src/ecoWatt_device.cpp`** 🟡 NEEDS UPDATE
**Current Code (Line 240+):**
```cpp
if (!uplink_packetizer_) {
    uplink_packetizer_ = new UplinkPacketizer(storage_, http_client_);
}

if (!remote_config_handler_) {
    remote_config_handler_ = new RemoteConfigHandler(config_, http_client_, command_executor_);
}
```

**Problem:** Passing plain `http_client_` instead of `secure_http_`

**What needs to change:**
- Pass `secure_http_` to constructors
- Update constructor signatures

---

### 4. **`include/remote_config_handler.hpp`** 🟡 HEADER UPDATE
**Current Constructor:**
```cpp
RemoteConfigHandler(ConfigManager* config, EcoHttpClient* http, CommandExecutor* cmd_executor);
```

**Needs to become:**
```cpp
RemoteConfigHandler(ConfigManager* config, SecureHttpClient* secure_http, CommandExecutor* cmd_executor);
```

---

### 5. **`include/uplink_packetizer.hpp`** 🟡 HEADER UPDATE
**Current Constructor:**
```cpp
UplinkPacketizer(DataStorage* storage, EcoHttpClient* http);
```

**Needs to become:**
```cpp
UplinkPacketizer(DataStorage* storage, SecureHttpClient* secure_http);
```

---

### Flask Backend Side (Python)

#### Files Requiring Changes:

### 6. **`app.py`** 🟢 ALREADY CONFIGURED (Verify)

**Security is ENABLED (Line 39):**
```python
SECURITY_ENABLED = True
PSK = bytes.fromhex("c41716a134168f52fbd4be3302fa5a88127ddde749501a199607b4c286ad29b3")
NONCE_STORE = {}
NONCE_WINDOW = 100
```

**Endpoints that need security verification:**
```python
@app.route('/api/inverter/config', methods=['GET'])        # Config polling
@app.route('/api/inverter/config/ack', methods=['POST'])   # Config ACK
@app.route('/api/upload', methods=['POST'])                # Data upload
@app.route('/api/upload/benchmark', methods=['POST'])      # Benchmark
```

**What needs verification:**
1. Check if `/api/inverter/config` endpoint verifies secured GET requests
2. Check if `/api/upload` endpoint verifies secured POST with binary data
3. Add security helper functions if missing:
   - `verify_hmac(payload_dict, received_mac)`
   - `check_nonce(device_id, nonce)`
   - `generate_secured_response(data)`

---

## Step-by-Step Fix Guide

### Phase 1: ESP32 Code Changes (C++)

#### Step 1.1: Update RemoteConfigHandler Header
**File:** `include/remote_config_handler.hpp`

```cpp
class RemoteConfigHandler {
public:
    // OLD: RemoteConfigHandler(ConfigManager* config, EcoHttpClient* http, CommandExecutor* cmd_executor);
    // NEW:
    RemoteConfigHandler(ConfigManager* config, SecureHttpClient* secure_http, CommandExecutor* cmd_executor);
    
private:
    ConfigManager* config_;
    // OLD: EcoHttpClient* http_;
    // NEW:
    SecureHttpClient* secure_http_;
    CommandExecutor* cmd_executor_;
    // ... rest unchanged
};
```

#### Step 1.2: Update RemoteConfigHandler Implementation
**File:** `src/remote_config_handler.cpp`

**Change Constructor:**
```cpp
RemoteConfigHandler::RemoteConfigHandler(ConfigManager* config, SecureHttpClient* secure_http, CommandExecutor* cmd_executor)
    : pollTicker_(pollTaskWrapper, 60000), config_(config), secure_http_(secure_http), cmd_executor_(cmd_executor) {
    instance_ = this;
}
```

**Change checkForConfigUpdate() method (Line ~53):**
```cpp
void RemoteConfigHandler::checkForConfigUpdate() {
    if (!secure_http_ || !config_) return;
    
    Logger::info("[RemoteCfg] Checking for config updates and commands from cloud...");
    
    // OLD: EcoHttpResponse resp = http_->get(config_->getApiConfig().config_endpoint.c_str());
    // NEW: Use secured GET
    std::string plain_response;
    EcoHttpResponse resp = secure_http_->secureGet(
        config_->getApiConfig().config_endpoint.c_str(), 
        plain_response
    );
    
    if (!resp.isSuccess()) {
        Logger::warn("[RemoteCfg] Failed to get config from cloud: status=%d", resp.status_code);
        return;
    }
    
    // Parse plain_response instead of resp.body
    ConfigUpdateRequest request;
    if (parseConfigUpdateRequest(plain_response.c_str(), request)) {
        // ... rest unchanged
    }
}
```

**Change sendConfigAck() method (Line ~194):**
```cpp
void RemoteConfigHandler::sendConfigAck(const ConfigUpdateAck& ack) {
    if (!secure_http_) return;
    
    // Serialize ACK to JSON
    std::string json = serializeConfigAck(ack);
    
    std::string ack_endpoint = config_->getApiConfig().config_endpoint + "/ack";
    
    // OLD: EcoHttpResponse resp = http_->post(ack_endpoint.c_str(), json.c_str(), json.length());
    // NEW: Use secured POST
    std::string plain_response;
    EcoHttpResponse resp = secure_http_->securePost(
        ack_endpoint.c_str(), 
        json, 
        plain_response
    );
    
    if (resp.isSuccess()) {
        Logger::info("[RemoteCfg] Config ACK sent successfully");
    } else {
        Logger::warn("[RemoteCfg] Failed to send config ACK: status=%d", resp.status_code);
    }
}
```

#### Step 1.3: Update UplinkPacketizer Header
**File:** `include/uplink_packetizer.hpp`

```cpp
class UplinkPacketizer {
public:
    // OLD: UplinkPacketizer(DataStorage* storage, EcoHttpClient* http);
    // NEW:
    UplinkPacketizer(DataStorage* storage, SecureHttpClient* secure_http);
    
private:
    DataStorage* storage_;
    // OLD: EcoHttpClient* http_;
    // NEW:
    SecureHttpClient* secure_http_;
    // ... rest unchanged
};
```

#### Step 1.4: Update UplinkPacketizer Implementation
**File:** `src/uplink_packetizer.cpp`

**Change Constructor:**
```cpp
UplinkPacketizer::UplinkPacketizer(DataStorage* storage, SecureHttpClient* secure_http)
    : uploadTicker_(uploadTaskWrapper, 60000), storage_(storage), secure_http_(secure_http) {
    instance_ = this;
}
```

**Change sendChunk() method:**
```cpp
bool UplinkPacketizer::sendChunk(const uint8_t* data, size_t size, size_t offset) {
    if (!secure_http_) return false;
    
    // Convert binary data to Base64 string
    std::string base64_payload = base64_encode(data, size);
    
    // Create JSON payload
    char json_buf[512];
    snprintf(json_buf, sizeof(json_buf), 
             "{\"offset\":%u,\"size\":%u,\"data\":\"%s\"}", 
             offset, size, base64_payload.c_str());
    
    Logger::info("[Uplink] Sending chunk: endpoint=%s, offset=%u, size=%u", 
                 cloud_endpoint_.c_str(), offset, size);
    
    // OLD: EcoHttpResponse resp = http_->post(cloud_endpoint_.c_str(), (const char*)data, size);
    // NEW: Use secured POST
    std::string plain_response;
    EcoHttpResponse resp = secure_http_->securePost(
        cloud_endpoint_.c_str(), 
        std::string(json_buf), 
        plain_response
    );
    
    return resp.isSuccess();
}
```

#### Step 1.5: Update EcoWattDevice Integration
**File:** `src/ecoWatt_device.cpp`

**Change initialization (around Line 240+):**
```cpp
if (!uplink_packetizer_) {
    // OLD: uplink_packetizer_ = new UplinkPacketizer(storage_, http_client_);
    // NEW: Pass secure_http_ instead
    uplink_packetizer_ = new UplinkPacketizer(storage_, secure_http_);
    uplink_packetizer_->setCloudEndpoint(api_conf.upload_endpoint);
    uplink_packetizer_->begin(15 * 1000);
    Logger::info("UplinkPacketizer initialized with security, upload interval: 15 seconds");
}

if (!remote_config_handler_) {
    // OLD: remote_config_handler_ = new RemoteConfigHandler(config_, http_client_, command_executor_);
    // NEW: Pass secure_http_ instead
    remote_config_handler_ = new RemoteConfigHandler(config_, secure_http_, command_executor_);
    using namespace std::placeholders;
    remote_config_handler_->onConfigUpdate([this]() { onConfigUpdated(); });
    remote_config_handler_->onCommand([this](const CommandRequest& cmd) { onCommandReceived(cmd); });
    remote_config_handler_->begin(60000);
    Logger::info("RemoteConfigHandler initialized with security, check interval: 60 seconds");
}
```

---

### Phase 2: Flask Backend Verification (Python)

#### Step 2.1: Verify Security Helper Functions
**File:** `app.py`

**Check if these functions exist:**
```python
def verify_hmac(payload_dict, received_mac):
    """Verify HMAC-SHA256 signature"""
    payload_copy = payload_dict.copy()
    if "mac" in payload_copy:
        del payload_copy["mac"]
    
    sorted_json = json.dumps(payload_copy, sort_keys=True)
    calculated_mac = hmac.new(PSK, sorted_json.encode(), hashlib.sha256).hexdigest()
    
    return calculated_mac == received_mac

def check_nonce(device_id, nonce):
    """Check if nonce is valid (not replayed)"""
    if device_id not in NONCE_STORE:
        NONCE_STORE[device_id] = nonce
        return True, "First nonce accepted"
    
    last_nonce = NONCE_STORE[device_id]
    
    if nonce <= last_nonce:
        return False, f"Replay detected: nonce {nonce} <= last_nonce {last_nonce}"
    
    if nonce > last_nonce + NONCE_WINDOW:
        return False, f"Nonce too far ahead: {nonce} > {last_nonce} + {NONCE_WINDOW}"
    
    NONCE_STORE[device_id] = nonce
    return True, f"Nonce accepted: {nonce}"
```

#### Step 2.2: Update `/api/inverter/config` Endpoint
**File:** `app.py`

```python
@app.route('/api/inverter/config', methods=['GET'])
def get_device_config():
    """Device polls for config updates (secured GET)"""
    device_id = request.headers.get('Device-ID', 'Unknown-Device')
    
    if SECURITY_ENABLED:
        # For GET requests, security headers: X-Nonce, X-Timestamp, X-MAC
        nonce = request.headers.get('X-Nonce')
        timestamp = request.headers.get('X-Timestamp')
        received_mac = request.headers.get('X-MAC')
        
        if not all([nonce, timestamp, received_mac]):
            return jsonify({"error": "Missing security headers"}), 401
        
        # Verify nonce
        nonce_valid, nonce_msg = check_nonce(device_id, int(nonce))
        if not nonce_valid:
            SECURITY_LOGS.append({
                "timestamp": datetime.datetime.now().isoformat(),
                "device_id": device_id,
                "event_type": "replay_attack",
                "details": nonce_msg
            })
            return jsonify({"error": "Replay attack detected"}), 403
        
        # Verify HMAC (compute over: endpoint + nonce + timestamp)
        endpoint = "/api/inverter/config"
        hmac_input = endpoint + str(nonce) + str(timestamp)
        calculated_mac = hmac.new(PSK, hmac_input.encode(), hashlib.sha256).hexdigest()
        
        if calculated_mac != received_mac:
            SECURITY_LOGS.append({
                "timestamp": datetime.datetime.now().isoformat(),
                "device_id": device_id,
                "event_type": "hmac_failed",
                "details": f"MAC mismatch"
            })
            return jsonify({"error": "HMAC verification failed"}), 403
    
    # Check for pending config/commands
    config_update = PENDING_CONFIGS.get(device_id)
    command = PENDING_COMMANDS.get(device_id)
    
    response_data = {}
    
    if config_update:
        response_data["config_update"] = config_update
    
    if command:
        response_data["command"] = command
    
    if not response_data:
        response_data = {"status": "no_updates"}
    
    # Generate secured response if security enabled
    if SECURITY_ENABLED:
        response_json = json.dumps(response_data, sort_keys=True)
        response_b64 = base64.b64encode(response_json.encode()).decode()
        
        response_nonce = int(nonce) + 1  # Server responds with next nonce
        response_envelope = {
            "nonce": response_nonce,
            "timestamp": int(time.time() * 1000),
            "encrypted": False,
            "payload": response_b64
        }
        
        response_mac = hmac.new(PSK, json.dumps(response_envelope, sort_keys=True).encode(), hashlib.sha256).hexdigest()
        response_envelope["mac"] = response_mac
        
        return jsonify(response_envelope), 200
    else:
        return jsonify(response_data), 200
```

#### Step 2.3: Update `/api/upload` Endpoint
**File:** `app.py`

```python
@app.route('/api/upload', methods=['POST'])
def upload_binary():
    """Receive secured binary data upload"""
    device_id = request.headers.get('Device-ID', 'Unknown-Device')
    
    if SECURITY_ENABLED:
        # Expect secured envelope in request body
        try:
            data = request.get_json()
        except:
            return jsonify({"error": "Invalid JSON"}), 400
        
        # Verify required fields
        required_fields = ["nonce", "timestamp", "payload", "mac"]
        if not all(f in data for f in required_fields):
            return jsonify({"error": "Missing secured envelope fields"}), 400
        
        nonce = data["nonce"]
        received_mac = data["mac"]
        
        # Check nonce (anti-replay)
        nonce_valid, nonce_msg = check_nonce(device_id, nonce)
        if not nonce_valid:
            SECURITY_LOGS.append({
                "timestamp": datetime.datetime.now().isoformat(),
                "device_id": device_id,
                "event_type": "replay_attack",
                "details": nonce_msg
            })
            return jsonify({"error": "Replay attack detected"}), 403
        
        # Verify HMAC
        hmac_valid = verify_hmac(data, received_mac)
        if not hmac_valid:
            SECURITY_LOGS.append({
                "timestamp": datetime.datetime.now().isoformat(),
                "device_id": device_id,
                "event_type": "hmac_failed",
                "details": "Upload HMAC verification failed"
            })
            return jsonify({"error": "HMAC verification failed"}), 403
        
        # Decode payload
        try:
            payload_b64 = data["payload"]
            payload_json = base64.b64decode(payload_b64).decode('utf-8')
            payload_data = json.loads(payload_json)
        except Exception as e:
            return jsonify({"error": f"Payload decode failed: {str(e)}"}), 400
        
        # Extract binary data from payload_data["data"]
        binary_data_b64 = payload_data.get("data", "")
        binary_data = base64.b64decode(binary_data_b64)
        offset = payload_data.get("offset", 0)
        size = payload_data.get("size", len(binary_data))
    else:
        # Plain upload (backward compatibility)
        binary_data = request.get_data()
        offset = 0
        size = len(binary_data)
    
    # Process binary data (existing decompression logic)
    # ... [existing code to handle delta decompression] ...
    
    return jsonify({"status": "success"}), 200
```

---

## Testing Strategy

### Test 1: Verify Security Initialization
```cpp
// ESP32 logs should show:
[INFO] Security Layer initialized successfully
[INFO] Secure HTTP Client initialized
[INFO] UplinkPacketizer initialized with security
[INFO] RemoteConfigHandler initialized with security
```

### Test 2: Verify Remote Config Works
```cpp
// Expected: 200 OK with secured response
[INFO] [RemoteCfg] Checking for config updates from cloud...
[INFO] [RemoteCfg] No config updates available
// NOT: [WARN] Failed to get config from cloud: status=401
```

### Test 3: Verify Upload Works
```cpp
// Expected: Successful upload
[INFO] [Uplink] Sending chunk: endpoint=http://..., offset=0, size=90
[INFO] [Uplink] Upload successful
// NOT: [WARN] UplinkPacketizer: failed to upload chunk
```

### Test 4: Check Flask Logs
```python
# Flask should show:
[SECURITY] MESSAGE_VERIFIED: {"device_id": "EcoWatt001", "nonce": 123}
[SECURITY] Nonce accepted: 124
[SECURITY] HMAC verification PASSED
```

---

## Implementation Priority

### 🔴 Critical (Do First):
1. ✅ Update `remote_config_handler.hpp` constructor signature
2. ✅ Update `remote_config_handler.cpp` to use `SecureHttpClient`
3. ✅ Update `ecoWatt_device.cpp` to pass `secure_http_` to RemoteConfigHandler
4. ✅ Verify Flask `/api/inverter/config` endpoint handles secured GET

### 🟡 High Priority (Do Second):
5. ✅ Update `uplink_packetizer.hpp` constructor signature
6. ✅ Update `uplink_packetizer.cpp` to use `SecureHttpClient`
7. ✅ Update `ecoWatt_device.cpp` to pass `secure_http_` to UplinkPacketizer
8. ✅ Update Flask `/api/upload` endpoint to handle secured binary uploads

### 🟢 Medium Priority (Verify):
9. ✅ Test end-to-end with real device
10. ✅ Check Node-RED dashboard displays data correctly
11. ✅ Verify security logs in Flask show no HMAC failures
12. ✅ Test config updates via Node-RED `/config` dashboard

---

## Expected Results After Fix

### ESP32 Serial Monitor:
```
[INFO] EcoWatt Device initializing...
[INFO] Security Layer initialized successfully
[INFO] Secure HTTP Client initialized
[INFO] UplinkPacketizer initialized with security
[INFO] RemoteConfigHandler initialized with security
[INFO] [RemoteCfg] Checking for config updates from cloud...
[DEBUG] [SecureHttp] Performing secure GET request
[DEBUG] [SecureHttp] GET with nonce=5, mac=51bba997...
[INFO] [RemoteCfg] No config updates available ✅
[INFO] [Uplink] Sending chunk with security ✅
[INFO] [Uplink] Upload successful ✅
```

### Flask Console:
```
[REQUEST] Received secured GET from EcoWatt001
[NONCE] Nonce accepted: 5
[HMAC] Verification PASSED ✅
[RESPONSE] Sending secured response with nonce 6

[REQUEST] Received secured upload from EcoWatt001
[NONCE] Nonce accepted: 7
[HMAC] Verification PASSED ✅
[SECURITY] MESSAGE_VERIFIED: {"device_id": "EcoWatt001", "nonce": 7}
```

### Node-RED Dashboard:
```
✅ Data appearing in dashboard
✅ Device status: Connected
✅ Latest upload: 2 seconds ago
✅ Security logs: No failures
```

---

## Files Summary

### Files to Modify:
1. ✅ `include/remote_config_handler.hpp` - Update constructor
2. ✅ `src/remote_config_handler.cpp` - Use SecureHttpClient
3. ✅ `include/uplink_packetizer.hpp` - Update constructor
4. ✅ `src/uplink_packetizer.cpp` - Use SecureHttpClient
5. ✅ `src/ecoWatt_device.cpp` - Pass secure_http_ to constructors
6. ⚠️ `app.py` - Verify/update security endpoints

### Build Commands:
```bash
cd "e:\ES Phase 4\embedded-systems-engineering-phase-4\cpp-esp"
pio run
pio run -t upload -t monitor
```

### Flask Start:
```bash
python app.py
```

### Node-RED Access:
```
http://localhost:1880/dashboard
http://localhost:1880/config
http://localhost:1880/commands
```

---

## Conclusion

The 401 errors and upload failures are caused by **security layer being enabled in Flask but not integrated into ESP32's HTTP clients**. The fix requires:

1. **ESP32:** Replace `EcoHttpClient` with `SecureHttpClient` in:
   - RemoteConfigHandler (config polling)
   - UplinkPacketizer (data uploads)

2. **Flask:** Verify security verification logic exists for:
   - `/api/inverter/config` (GET with security headers)
   - `/api/upload` (POST with secured envelope)

Once these changes are made, the ESP32 will successfully communicate with the secured Flask backend, and Node-RED will display the data correctly.

**Estimated Implementation Time:** 30-45 minutes
**Difficulty:** Medium (requires careful refactoring but straightforward)
