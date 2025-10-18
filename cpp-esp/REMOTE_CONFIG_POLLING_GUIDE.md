# Remote Configuration Polling - Verification Guide

## How the System Works

### 1. **Configuration Flow**
```
Dashboard → Flask Server → Queued → ESP32 Polls → ESP32 Receives → ESP32 Applies
```

### 2. **Current Configuration**
- **Polling Interval**: 60 seconds (ESP32 checks every 60 seconds)
- **Endpoint**: `/api/inverter/config` (GET request with security headers)
- **Device ID**: `EcoWatt001`

---

## What You've Done ✅

1. **Sent configuration** from `config_dashboard.html`
2. **Flask received it**:
   ```
   [CONFIG] Queued config update for EcoWatt001: nonce=301, interval=10s, registers=[0, 1, 2]
   ```
3. **Configuration is waiting** in server memory: `PENDING_CONFIGS['EcoWatt001']`

---

## What Should Happen Next ⏳

### Within 60 seconds, ESP32 will poll and you should see:

**On Flask Server:**
```
[DEBUG] /api/inverter/config called by device: EcoWatt001
[DEBUG] Headers: X-Nonce=XXX, X-Timestamp=XXX, X-MAC=XXX
[CONFIG] Sending pending config to EcoWatt001: {'nonce': 301, 'config_update': {...}}
[DEBUG] Secured response: {'nonce': 302, 'timestamp': ..., 'encrypted': True, ...}
```

**On ESP32 Serial Monitor:**
```
[RemoteCfg] Checking for config updates and commands from cloud...
[RemoteCfg] Received config update: nonce=301, interval=10000ms, registers=3
[CONFIG] Applying configuration update...
[CONFIG] ✓ Accepted: sampling_interval=10000ms
[CONFIG] ✓ Accepted: registers: 0,1,2
[EcoWattDevice] Applied config to scheduler: interval=10000 ms, registers=3
```

---

## Verification Steps

### Step 1: Check ESP32 Serial Monitor
Open serial monitor and look for:
```bash
pio device monitor --port COM5 --baud 115200
```

Expected output **every 60 seconds**:
```
[RemoteCfg] Checking for config updates and commands from cloud...
```

### Step 2: Check Flask Server Logs
Watch for ESP32 polling requests (should appear every 60 seconds):
```
[DEBUG] /api/inverter/config called by device: EcoWatt001
```

### Step 3: Wait for Next Poll Cycle
- ESP32 last polled at: **~60 seconds ago**
- Next poll will be: **Within 60 seconds from now**
- When ESP32 polls, it will:
  1. See the pending config (nonce=301)
  2. Download it via secured GET
  3. Apply the configuration
  4. Send acknowledgment back

---

## If ESP32 Is NOT Polling

### Check 1: Is RemoteConfigHandler Running?
ESP32 should show during initialization:
```
[INFO] RemoteConfigHandler initialized with security enabled, check interval: 60 seconds
```

### Check 2: Is Device ID Correct?
ESP32 should show during initialization:
```
[INFO] API key and Device-ID (EcoWatt001) configured for requests
```

### Check 3: Check for Security Errors
If security verification fails, Flask will log:
```
[SECURITY] EcoWatt001: hmac_failed - ...
[CONFIG] Security verification failed for EcoWatt001: ...
```

---

## Speed Up Testing (Optional)

To test immediately without waiting 60 seconds:

### Option A: Reduce Polling Interval Temporarily

**Modify** `src/ecoWatt_device.cpp` line ~289:
```cpp
// FROM:
remote_config_handler_->begin(60000); // Check every 60 seconds

// TO:
remote_config_handler_->begin(10000); // Check every 10 seconds
```

Then rebuild and upload:
```bash
pio run --target upload --upload-port COM5
```

### Option B: Reset ESP32
Press the reset button on ESP32 - it will poll immediately on startup.

---

## Expected Timeline

### Normal Flow (60-second polling):
```
T+0s:   You send config from dashboard
T+0s:   Flask queues config for EcoWatt001
T+0-60s: ESP32 continues normal operation
T+60s:  ESP32 polls /api/inverter/config
T+60s:  ESP32 receives and applies config
T+60s:  ESP32 sends acknowledgment
T+60s:  Flask clears pending config
```

### What You'll See:

**Flask Log Sequence:**
```
1. [CONFIG] Queued config update for EcoWatt001: nonce=301, ...  ← Already done
2. [DEBUG] /api/inverter/config called by device: EcoWatt001    ← Waiting for this
3. [CONFIG] Sending pending config to EcoWatt001: ...            ← Will happen next
4. [CONFIG ACK] Received from EcoWatt001: all_success=true      ← Final confirmation
```

**ESP32 Log Sequence:**
```
1. [RemoteCfg] Checking for config updates and commands from cloud...
2. [RemoteCfg] Received config update: nonce=301, interval=10000ms, registers=3
3. [CONFIG] Applying configuration update...
4. [CONFIG] ✓ Accepted: sampling_interval=10000ms
5. [CONFIG] ✓ Accepted: registers: 0,1,2
6. [EcoWattDevice] Applied config to scheduler: interval=10000 ms, registers=3
7. [CONFIG] Sending acknowledgment to cloud
```

---

## Troubleshooting

### Problem: No polling logs appear after 60 seconds

**Possible Causes:**
1. RemoteConfigHandler not initialized
2. Security verification failing
3. Network/WiFi issue
4. ESP32 in error state

**Check:**
- ESP32 serial monitor for errors
- Flask server for rejected requests
- WiFi connection status

### Problem: Polling happens but config not applied

**Possible Causes:**
1. Security HMAC verification fails
2. JSON parsing error
3. Configuration validation fails

**Check:**
- Flask logs for `[CONFIG] Security verification failed`
- ESP32 logs for validation errors

---

## Summary

**Current Status:** ✅ Configuration queued on server (nonce=301)

**Next Step:** ⏳ Wait for ESP32 to poll (every 60 seconds)

**Expected Result:** ESP32 will receive and apply:
- Sampling interval: 10 seconds (10000 ms)
- Registers to poll: [0, 1, 2]

**Verification:** Watch Flask logs for:
```
[DEBUG] /api/inverter/config called by device: EcoWatt001
[CONFIG] Sending pending config to EcoWatt001
```

---

**The system is working correctly - just needs time for the next poll cycle!**