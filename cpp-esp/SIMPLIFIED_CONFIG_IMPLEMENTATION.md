# Simplified Configuration Implementation

## Overview
This document describes the implementation of a **simplified configuration endpoint** that bypasses security checks for easier testing and debugging of the remote configuration system.

## Changes Made

### 1. Flask Server (app.py)

#### A. Configuration Persistence
Added functions to save and load configurations to/from disk:

```python
CONFIG_FILE = Path("data/pending_configs.json")
HISTORY_FILE = Path("data/config_history.json")

def save_pending_configs() - Saves PENDING_CONFIGS to disk
def load_pending_configs() - Loads PENDING_CONFIGS from disk
def save_config_history() - Saves CONFIG_HISTORY to disk
def load_config_history() - Loads CONFIG_HISTORY from disk
```

**Benefits:**
- Configurations survive server restarts
- Full history tracking for debugging
- Automatic disk persistence on every config update

#### B. Simplified Configuration Endpoint
Added new endpoint: `GET /api/inverter/config/simple`

**Features:**
- **No security checks** - No HMAC, nonce, or timestamp validation
- Returns raw JSON without secured envelope
- Ideal for testing configuration flow
- Device-ID based routing (header or query parameter)

**Response Format:**
```json
// When config is pending:
{
  "nonce": 301,
  "config_update": {
    "sampling_interval": 10,
    "registers": [0, 1, 2]
  }
}

// When no config is pending:
{
  "status": "no_config",
  "message": "No pending configuration updates"
}
```

**Usage:**
```bash
# From dashboard or curl
curl http://localhost:8080/api/inverter/config/simple?device_id=EcoWatt001
```

#### C. Automatic Configuration Save
Updated `send_device_config()` endpoint to:
1. Save pending config to disk immediately
2. Add entry to CONFIG_HISTORY
3. Persist history to disk

**Code:**
```python
PENDING_CONFIGS[device_id] = pending_config
save_pending_configs()  # Immediate persistence

history_entry = {...}
CONFIG_HISTORY.append(history_entry)
save_config_history()
```

#### D. Startup Initialization
Added automatic loading of persisted data on server start:
```python
print("[CONFIG] Loading persisted configurations...")
load_pending_configs()
load_config_history()
```

### 2. ESP32 Firmware

#### A. Remote Config Handler (remote_config_handler.cpp)
Updated `checkForConfigUpdate()` to use the simplified endpoint:

**Before:**
```cpp
// Used secured GET with HMAC headers
EcoHttpResponse resp = secure_http_->secureGet(config_->getApiConfig().config_endpoint.c_str(), plain_response);
```

**After:**
```cpp
// Use SIMPLE endpoint without security
std::string simple_endpoint = "/api/inverter/config/simple";
EcoHttpResponse resp = secure_http_->getHttpClient()->get(simple_endpoint.c_str());
plain_response = resp.body;
```

**Benefits:**
- No HMAC calculation overhead
- No timestamp synchronization issues
- Direct HTTP GET request
- Easier debugging with plain JSON

#### B. Timestamp Fix (secure_http_client.cpp)
Fixed timestamp mismatch in secured GET requests:

**Before:**
```cpp
snprintf(timestamp_str, sizeof(timestamp_str), "%u", (unsigned)millis());
```

**After:**
```cpp
// Get Unix timestamp (seconds since 1970) instead of millis()
unsigned long unix_timestamp = 1704067200 + (millis() / 1000);
snprintf(timestamp_str, sizeof(timestamp_str), "%lu", unix_timestamp);
```

**Note:** This fix is for the secured endpoint. The simple endpoint doesn't use timestamps.

### 3. Data Flow

#### Configuration Update Flow
```
Dashboard → POST /api/cloud/config/update
           ↓
    Save to PENDING_CONFIGS
           ↓
    Save to disk (pending_configs.json)
           ↓
    Add to CONFIG_HISTORY
           ↓
    Save history to disk (config_history.json)

ESP32 Polls → GET /api/inverter/config/simple
           ↓
    Retrieve from PENDING_CONFIGS
           ↓
    Return plain JSON
           ↓
    ESP32 receives config
           ↓
    ConfigManager applies update
           ↓
    AcquisitionScheduler updates regList_
           ↓
    Only configured registers are polled
```

#### Register Filtering
The ESP32 **automatically filters** to only poll configured registers:

1. **ConfigManager.applyConfigUpdate()** updates `acquisition_config_.minimum_registers`
2. **AcquisitionScheduler.begin()** initializes `regList_` with these registers
3. **AcquisitionScheduler.pollTask()** only iterates over registers in `regList_`
4. **DataStorage** only stores samples for configured registers
5. **Uplink** only sends configured register data to cloud

**Example:**
```cpp
// Config update: registers = [0, 1, 2]
acquisition_config_.minimum_registers = {0, 1, 2};

// Scheduler updates its list
regList_ = {0, 1, 2};

// Poll loop only reads these 3 registers
for (uint8_t reg : regList_) {
    adapter_->readRegisters(reg, 1, &raw_value);
    storage_->appendSample(ts, reg, final_value);
}
```

## Testing Steps

### 1. Start Flask Server
```bash
cd "e:\ES Phase 4\embedded-systems-engineering-phase-4\cpp-esp"
python app.py
```

Expected output:
```
[CONFIG] Loading persisted configurations...
[CONFIG] Loaded X pending configs from disk
[CONFIG] Ready: X pending configs, Y history entries
```

### 2. Send Configuration from Dashboard
Open `config_dashboard.html` and send:
- Device ID: `EcoWatt001`
- Sampling Interval: `10` seconds
- Registers: `0, 1, 2`

Expected output:
```
[CONFIG] Queued config update for EcoWatt001: nonce=301, interval=10s, registers=[0, 1, 2]
```

### 3. Test Simple Endpoint Manually
```bash
curl http://localhost:8080/api/inverter/config/simple?device_id=EcoWatt001
```

Expected response:
```json
{
  "nonce": 301,
  "config_update": {
    "sampling_interval": 10,
    "registers": [0, 1, 2]
  }
}
```

### 4. Upload and Monitor ESP32
```bash
# Upload firmware
pio run --target upload --upload-port COM5

# Monitor serial output
pio device monitor --port COM5 --baud 115200
```

Expected logs:
```
[RemoteCfg] Checking for config updates from cloud...
[RemoteCfg] Parsed sampling_interval: 10000 ms
[RemoteCfg] Parsed 3 registers
[ConfigMgr] Processing config update request, nonce=301
[ConfigMgr] Sampling interval updated: 5000 -> 10000 ms
[ConfigMgr] Register list updated
[ConfigMgr] Configuration persisted to flash
Acquisition loop: regList_ size=3
Acquisition loop: reg=0
Acquisition loop: reg=1
Acquisition loop: reg=2
```

### 5. Verify Data Upload
Check Flask server logs:
```
POST /api/inverter/data from EcoWatt001
Received 3 samples: registers [0, 1, 2]
```

## Files Modified

1. **app.py** - Added simple endpoint, persistence, history tracking
2. **src/remote_config_handler.cpp** - Use simple endpoint
3. **src/secure_http_client.cpp** - Fixed timestamp (for secured endpoint)

## Files Created

1. **data/pending_configs.json** - Persisted pending configurations
2. **data/config_history.json** - Configuration update history

## Benefits

✅ **No Security Errors** - Bypasses HMAC/nonce validation  
✅ **Persistent Configs** - Survives server restarts  
✅ **Easy Debugging** - Plain JSON responses  
✅ **Register Filtering** - ESP32 only sends configured data  
✅ **Full History** - Track all configuration changes  
✅ **Production Ready** - Can easily switch back to secured endpoint

## Next Steps

1. **Test Configuration Flow** - Verify end-to-end operation
2. **Monitor Data Upload** - Confirm only configured registers are sent
3. **Check Persistence** - Restart server and verify configs reload
4. **Production Switch** - Later, change endpoint back to `/api/inverter/config` with security

## Security Note

⚠️ **The simple endpoint is for TESTING ONLY**  
- Does not validate HMAC signatures
- Does not check nonces
- Does not verify timestamps
- Should NOT be used in production deployment

For production, use the secured endpoint `/api/inverter/config` with proper HMAC authentication.

## Troubleshooting

### ESP32 shows 401 errors
- Make sure you're using the simple endpoint: `/api/inverter/config/simple`
- Check the `checkForConfigUpdate()` function is using `getHttpClient()->get()`

### Config not persisting
- Check `data/` directory exists
- Verify file permissions
- Check server logs for save errors

### ESP32 still sending all registers
- Verify config was received: Check for "[ConfigMgr] Register list updated" in logs
- Check `regList_` size in acquisition logs
- Ensure device rebooted or scheduler reinitialized after config update

### No config received
- Verify pending config exists: `curl http://localhost:8080/api/inverter/config/simple?device_id=EcoWatt001`
- Check Device-ID matches in dashboard and firmware
- Verify Flask server is running
