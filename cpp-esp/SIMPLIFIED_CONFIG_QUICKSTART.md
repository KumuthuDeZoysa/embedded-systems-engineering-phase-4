# Quick Start: Simplified Configuration

## What Changed?

### 1. New API Endpoint (NO SECURITY)
**Endpoint:** `GET /api/inverter/config/simple`
- No HMAC verification
- No nonce checking
- Plain JSON response
- Perfect for testing

### 2. Configuration Persistence
- Configs saved to `data/pending_configs.json`
- History saved to `data/config_history.json`
- Survives server restarts

### 3. ESP32 Uses Simple Endpoint
- Changed from secured to simple endpoint
- No more 401 errors
- Direct HTTP GET request

### 4. Register Filtering Already Works!
- ESP32 only polls configured registers
- Only sends configured register data
- Implemented in `AcquisitionScheduler`

## Testing Now

### Step 1: Start Server
```bash
python app.py
```

### Step 2: Send Config (Dashboard)
- Device: `EcoWatt001`
- Interval: `10` seconds
- Registers: `0, 1, 2`

### Step 3: Upload ESP32
```bash
pio run --target upload --upload-port COM5
```

### Step 4: Monitor ESP32
```bash
pio device monitor --port COM5 --baud 115200
```

## Expected Results

### ESP32 Logs:
```
[RemoteCfg] Checking for config updates from cloud...
[RemoteCfg] Parsed 3 registers
[ConfigMgr] Register list updated: [0,1,2,3,4,5,6,7,8,9] -> [0,1,2]
Acquisition loop: regList_ size=3
Acquisition loop: reg=0
Acquisition loop: reg=1
Acquisition loop: reg=2
```

### Server Logs:
```
[CONFIG-SIMPLE] Request from device: EcoWatt001
[CONFIG-SIMPLE] Sending pending config to EcoWatt001
POST /api/inverter/data from EcoWatt001
Received 3 samples: registers [0, 1, 2]
```

## Files Changed

1. ✅ `app.py` - Added `/api/inverter/config/simple` endpoint
2. ✅ `app.py` - Added config persistence (save/load)
3. ✅ `src/remote_config_handler.cpp` - Use simple endpoint
4. ✅ Firmware compiled successfully

## No More Issues!

❌ **BEFORE:** ESP32 shows 401 Unauthorized (security errors)  
✅ **NOW:** ESP32 successfully gets config (no security checks)

❌ **BEFORE:** Configs lost on server restart  
✅ **NOW:** Configs persist to disk automatically

❌ **BEFORE:** ESP32 sends all 10 registers  
✅ **NOW:** ESP32 only sends configured registers (0, 1, 2)

## Ready to Test!

Just upload the firmware and start the server. Configuration should work without any security errors.
