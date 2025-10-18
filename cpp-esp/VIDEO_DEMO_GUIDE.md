# 🎥 Milestone 4 Video Demonstration Guide

## Setup Before Recording

### 1. Start Flask Server
```powershell
cd "E:\ES Phase 4\embedded-systems-engineering-phase-4\cpp-esp"
python app.py
```
Wait until you see: `Running on http://10.50.126.183:8080`

### 2. Upload ESP32 Firmware & Start Serial Monitor
```powershell
pio run -e esp32dev -t upload -t monitor
```

### 3. Open Configuration Dashboard
Double-click: `config_dashboard.html`

---

## Part 1: Remote Configuration Demo (2 minutes)

### Screen Layout for Recording:
```
┌─────────────────────────────────────────────────┐
│  Configuration Dashboard (Left Side)            │
│  ┌────────────────────────────────┐            │
│  │ Device ID: EcoWatt001          │            │
│  │ Interval: 5 → 10 seconds       │            │
│  │ Registers: [0, 1] → [0, 1, 8]  │            │
│  └────────────────────────────────┘            │
├─────────────────────────────────────────────────┤
│  Serial Monitor (Right Side)                    │
│  Showing ESP32 logs in real-time               │
└─────────────────────────────────────────────────┘
```

### Narration & Actions:

**[0:30] Introduction**
> "Let's start with Part 1: Remote Configuration Management. 
> This allows us to update device parameters without rebooting."

**Action:** Show the config dashboard on screen

---

**[0:45] Show Current Configuration**
> "Currently, the device is sampling every 5 seconds and monitoring 
> two registers: Voltage (Reg 0) and Current (Reg 1)."

**Action:** 
- Point to Serial Monitor showing:
  ```
  [CONFIG] Current sampling interval: 5000ms
  [CONFIG] Active registers: [0, 1]
  ```

---

**[1:00] Navigate to Configuration Dashboard**
> "Now I'll use the Flask cloud dashboard to update the configuration. 
> I'm changing the sampling interval from 5 seconds to 10 seconds, 
> and adding a third register - address 8, which monitors export power percentage."

**Action:**
1. Type `10` in "Sampling Interval" field
2. Check the checkbox for "Reg 8: Export Power %"
3. Show the JSON payload preview updating:
   ```json
   {
     "device_id": "EcoWatt001",
     "sampling_interval": 10,
     "registers": [0, 1, 8]
   }
   ```

---

**[1:20] Send Configuration Update**
> "Let me send this configuration update to the cloud."

**Action:** 
- Click "Send Configuration to Cloud" button
- Show success message with nonce appearing

---

**[1:30] Device Receives & Validates**
> "Watch the device logs - it immediately receives the configuration, 
> validates it, and applies the changes without rebooting."

**Action:** 
- Focus on Serial Monitor showing:
  ```
  [INFO] Config update received from cloud
  [CONFIG] New interval: 10000ms (was 5000ms)
  [CONFIG] New registers: [0,1,8] (was [0,1])
  [CONFIG] Validation: PASSED
  [CONFIG] Applied successfully - no reboot needed
  [INFO] Sending acknowledgment to cloud
  ```

---

**[1:50] Verify Configuration Applied**
> "The configuration is now active. Notice the sampling now occurs 
> every 10 seconds instead of 5, and we're monitoring three registers 
> including the new export power percentage."

**Action:**
- Show Serial Monitor with new sampling behavior:
  ```
  [DATA] Sample #1: Reg0=230.0V, Reg1=2.5A, Reg8=75.0%
  [DATA] Next sample in 10000ms
  ```

---

**[2:15] Show Cloud Acknowledgment**
> "The cloud receives the acknowledgment confirming successful update."

**Action:**
- Show Flask logs:
  ```
  [CONFIG] EcoWatt001: config_applied - Changes: interval, registers
  POST /api/inverter/config/ack HTTP/1.1" 200
  ```

---

**[2:25] Summary**
> "Configuration updated successfully - no downtime, no reboot required. 
> This is the power of runtime configuration management."

**Action:** Quick recap screen showing before/after comparison

---

## Part 2: Command Execution Demo (1.5 minutes)

### Screen Layout:
```
┌─────────────────────────────────────────────────┐
│  Configuration Dashboard (Top)                   │
│  [Commands Tab Selected]                         │
│  ┌────────────────────────────────┐            │
│  │ Action: Write Register          │            │
│  │ Target: Reg 8 (Export Power %)  │            │
│  │ Value: 50                        │            │
│  └────────────────────────────────┘            │
├─────────────────────────────────────────────────┤
│  Serial Monitor (Bottom)                         │
│  Showing command execution round-trip            │
└─────────────────────────────────────────────────┘
```

### Narration & Actions:

**[2:30] Introduction**
> "Part 2: Command Execution. The cloud can send commands to the device, 
> which executes them on the Modbus inverter and reports results back."

---

**[2:45] Queue Command in Cloud**
> "I'm sending a write command to set register 8 - export power percentage - to 50%."

**Action:**
1. Open Commands section in dashboard
2. Select "Write Register"
3. Choose "Reg 8 (Export Power %)"
4. Enter value: 50
5. Click "Send Command"

---

**[3:00] Device Receives & Executes**
> "The device receives the command, validates it, and executes it on the inverter."

**Action:** Show Serial Monitor:
```
[CMD] Command received: WRITE_REG, reg=8, value=50.0
[CMD] Executing on Modbus inverter...
[CMD] Write successful: Reg8 = 50.0%
[CMD] Reporting result to cloud
```

---

**[3:20] Cloud Receives Result**
> "The cloud receives confirmation that the command executed successfully."

**Action:** Show Flask logs:
```
[COMMAND] EcoWatt001: command_executed - Action: write_register, Result: success
POST /api/cloud/command/result HTTP/1.1" 200
```

---

**[3:45] Summary**
> "Round-trip complete: cloud → device → inverter → device → cloud. 
> Full traceability of command execution."

---

## Part 3: Security Layer Demo (2 minutes)

### Screen Setup:
```
┌─────────────────────────────────────────────────┐
│  Browser DevTools (Network Tab)                 │
│  Showing request headers with HMAC & nonce      │
├─────────────────────────────────────────────────┤
│  Flask Logs (Bottom)                             │
│  Showing security verification                   │
└─────────────────────────────────────────────────┘
```

### Narration & Actions:

**[4:00] Introduction**
> "Part 3: Security Layer. Every communication is authenticated using 
> HMAC-SHA256 and protected against replay attacks with nonces."

---

**[4:15] Show Secure Request**
> "When the device sends data, it includes three security headers: 
> X-Nonce for replay protection, X-Timestamp, and X-MAC for authentication."

**Action:** 
- Send a data upload from device
- Show Serial Monitor:
  ```
  [SECURITY] Generating HMAC for upload
  [SECURITY] Nonce: 145, Timestamp: 1729115820
  [SECURITY] MAC: a3f2b8c1d4e5f6...
  ```

---

**[4:30] Flask Verifies Security**
> "The Flask server verifies the HMAC signature and checks the nonce 
> hasn't been seen before."

**Action:** Show Flask logs:
```
[SECURITY] EcoWatt001: hmac_verified - Upload authenticated, nonce: 145
[SECURITY] Nonce 145 accepted (new)
POST /api/upload HTTP/1.1" 200
```

---

**[4:50] Simulate Replay Attack**
> "Now let's simulate a replay attack by resending the same message 
> with a duplicate nonce."

**Action:** 
1. Create a simple Python script or use curl to replay the request
2. Show the same nonce being sent again

---

**[5:10] Attack Blocked**
> "The server detects the duplicate nonce and rejects the request 
> with 401 Unauthorized."

**Action:** Show Flask logs:
```
[SECURITY] EcoWatt001: replay_attack_detected - Duplicate nonce: 145
[SECURITY] Request rejected: 401 Unauthorized
POST /api/upload HTTP/1.1" 401
```

---

**[5:30] Show HMAC Tampering Protection**
> "If an attacker modifies the payload, the HMAC verification fails."

**Action:**
- Send request with wrong HMAC
- Show Flask logs:
```
[SECURITY] EcoWatt001: hmac_failed - Invalid signature
POST /api/upload HTTP/1.1" 401
```

---

**[6:00] Summary**
> "All communications are cryptographically secured with replay protection. 
> No unauthorized access possible."

---

## Part 4a: FOTA Successful Update (2 minutes)

### Screen Setup:
```
┌─────────────────────────────────────────────────┐
│  Serial Monitor (Full Screen)                    │
│  Showing chunk-by-chunk download progress        │
└─────────────────────────────────────────────────┘
```

### Narration & Actions:

**[6:15] Introduction**
> "Part 4: Firmware Over-The-Air updates. The device can download 
> and install new firmware remotely with integrity verification."

---

**[6:30] Show Current Version**
> "Currently running version 1.0.0."

**Action:** Show Serial Monitor:
```
[INFO] EcoWatt Device v1.0.0 (Oct 16 2025)
[FOTA] Current firmware version: 1.0.0
```

---

**[6:45] Firmware Available**
> "A new version 1.0.3 is available on the cloud. 
> The device detects it and starts downloading."

**Action:** Show Serial Monitor:
```
[FOTA] Checking for updates...
[FOTA] New firmware available: v1.0.3 (32768 bytes, 32 chunks)
[FOTA] Starting download...
```

---

**[7:00] Chunked Download with Progress**
> "The firmware downloads in 1KB chunks with HMAC verification per chunk."

**Action:** Show Serial Monitor with progress:
```
[FOTA] Chunk 0/32 downloaded ✓ (3%)
[FOTA] Chunk 1/32 downloaded ✓ (6%)
[FOTA] Chunk 2/32 downloaded ✓ (9%)
...
[FOTA] Chunk 31/32 downloaded ✓ (100%)
[FOTA] All chunks downloaded
```

---

**[7:30] Integrity Verification**
> "After download completes, the device verifies the firmware 
> integrity using SHA-256 hash."

**Action:** Show Serial Monitor:
```
[FOTA] Verifying firmware integrity...
[FOTA] Expected hash: dd650cca4b885c3f...
[FOTA] Calculated hash: dd650cca4b885c3f...
[FOTA] ✓ Hash matches - firmware verified
```

---

**[7:50] Apply Update & Reboot**
> "Firmware verified successfully. The device applies the update 
> and reboots to activate the new version."

**Action:** Show Serial Monitor:
```
[FOTA] Applying firmware update...
[FOTA] Update written to flash
[FOTA] Rebooting in 3 seconds...
[FOTA] Reboot...

--- Reboot ---

[INFO] EcoWatt Device v1.0.3 (Oct 16 2025)
[FOTA] Boot successful on new firmware
[FOTA] Reporting success to cloud
```

---

**[8:10] Summary**
> "FOTA update successful! The device is now running version 1.0.3."

---

## Part 4b: FOTA Failed Update with Rollback (1.5 minutes)

### Narration & Actions:

**[8:15] Introduction**
> "Now let's see what happens when a firmware update fails. 
> This could be due to corruption or hash mismatch."

---

**[8:25] Upload Corrupted Firmware**
> "I'll upload a firmware with an incorrect hash to simulate corruption."

**Action:**
- Use `fota_test_upload.py` with intentionally wrong hash
- Show upload success

---

**[8:35] Device Downloads Corrupted Firmware**
> "The device downloads all chunks successfully..."

**Action:** Show Serial Monitor:
```
[FOTA] Downloading v1.0.4 (corrupted)...
[FOTA] Chunk 0/32 downloaded ✓
[FOTA] Chunk 1/32 downloaded ✓
...
[FOTA] Chunk 31/32 downloaded ✓
[FOTA] All chunks downloaded
```

---

**[8:50] Hash Mismatch Detected**
> "But during verification, the hash doesn't match. 
> The device detects corruption."

**Action:** Show Serial Monitor:
```
[FOTA] Verifying firmware integrity...
[FOTA] Expected hash: abc123def456...
[FOTA] Calculated hash: dd650cca4b88...
[FOTA] ✗ Hash mismatch - firmware corrupted!
[FOTA] Update REJECTED
```

---

**[9:05] Safe Rollback**
> "The device safely rejects the corrupted firmware and stays 
> on the current working version. No reboot, no downtime."

**Action:** Show Serial Monitor:
```
[FOTA] Rollback: Staying on v1.0.3
[FOTA] Device remains operational
[FOTA] Reporting failure to cloud
[INFO] System stable on v1.0.3
```

---

**[9:20] Cloud Receives Failure Report**
> "The cloud receives notification that the update failed."

**Action:** Show Flask logs:
```
[FOTA] EcoWatt001: update_failed - Reason: Hash mismatch
[FOTA] Device rolled back to v1.0.3
POST /api/inverter/fota/status HTTP/1.1" 200
```

---

**[9:35] Summary**
> "FOTA rollback successful! The device protected itself from 
> corrupted firmware and remained operational."

---

## Closing (15 seconds)

**[9:45] Final Summary**
> "We've demonstrated all four Milestone 4 features:
> 1. Remote Configuration with zero downtime
> 2. Command Execution with full traceability
> 3. Security Layer with HMAC and replay protection
> 4. FOTA with integrity verification and safe rollback
> 
> Thank you for watching!"

**Action:** Show final slide with all four checkmarks

---

## 📋 Pre-Recording Checklist

### Hardware & Software
- [ ] ESP32 connected to COM5
- [ ] USB cable secure (no disconnections during recording)
- [ ] Flask server running on http://10.50.126.183:8080
- [ ] Serial monitor ready (baud 115200)

### Files Ready
- [ ] `config_dashboard.html` open in browser
- [ ] `fota_test_upload.py` ready for Part 4b
- [ ] Screen recording software tested
- [ ] Audio levels checked

### ESP32 Preparation
- [ ] Flash firmware v1.0.0
- [ ] Set config to: interval=5s, registers=[0,1]
- [ ] Clear `/boot_count.txt` and `/fota_state.json`
- [ ] Verify WiFi connection to network

### Flask Server Preparation
- [ ] Upload firmware v1.0.3 (32KB, 32 chunks, correct hash)
- [ ] Clear all history (configs, commands, FOTA status)
- [ ] Prepare corrupted firmware for Part 4b

### Screen Layout
- [ ] Split screen: Dashboard (left) + Serial Monitor (right)
- [ ] Font size readable (12pt+)
- [ ] Terminal colors visible (dark background preferred)
- [ ] Hide taskbar/distractions

### Timing
- [ ] Practice run under 10 minutes
- [ ] Allow 2s buffer between parts
- [ ] Speak clearly and at moderate pace
- [ ] Pause for visual confirmations

---

## 🎬 Recording Tips

1. **Speak while showing**: Narrate what's happening on screen simultaneously
2. **Zoom when needed**: Zoom in on important logs/messages
3. **Highlight cursor**: Use cursor to point to key information
4. **Pause for effect**: Give viewers time to read logs (2-3 seconds)
5. **Steady hands**: Use mouse slowly and deliberately
6. **Clear audio**: Test microphone before recording
7. **No interruptions**: Close notifications, set phone to silent
8. **B-roll ready**: Have backup footage if something fails

---

## 🔧 Troubleshooting During Recording

### If ESP32 crashes:
- Don't panic! Say: "The device encountered an error and is rebooting"
- Show the reboot logs
- Continue from where you left off

### If Flask server stops:
- Restart it quickly
- Say: "I'm restarting the cloud server"
- Resume demonstration

### If WiFi disconnects:
- Check connection
- Say: "Reconnecting to network..."
- Wait for connection to restore

### If timing runs over:
- Cut introduction shorter (aim for 20 seconds instead of 30)
- Speed up command execution (Part 2 can be 1 min instead of 1.5)
- Keep security demo concise (1.5 min instead of 2)

---

## 📊 Expected Results Summary

| Part | Feature | Time | Expected Result |
|------|---------|------|----------------|
| 1 | Remote Config | 2:00 | Config updated: 5s→10s, [0,1]→[0,1,8] |
| 2 | Command Exec | 1:30 | Write Reg8=50%, round-trip confirmed |
| 3 | Security | 2:15 | HMAC verified, replay attack blocked |
| 4a | FOTA Success | 2:00 | v1.0.0 → v1.0.3 upgrade successful |
| 4b | FOTA Rollback | 1:30 | Corrupted firmware rejected, rolled back |
| **Total** | | **9:15** | **All 4 features demonstrated** |

---

Good luck with your recording! 🎥✨
