# 🎬 Serial Monitor Solutions for Video Demo

## Problem
Serial logs scroll too fast to see important information during video recording.

## Solutions

### ✅ Solution 1: Use Interactive Serial Monitor (RECOMMENDED)
**Best for: Live recording with pause/resume control**

#### Install dependencies:
```powershell
pip install pyserial colorama
```

#### Run:
```powershell
python serial_monitor_interactive.py
```

#### Controls:
- **SPACE** - Pause/Resume logs (freeze screen during narration)
- **F** - Toggle filter on/off (show/hide debug logs)
- **Q** - Quit

#### Benefits:
- ✅ Color-coded logs (errors=red, success=green, config=cyan)
- ✅ Pause anytime to narrate
- ✅ Filter out noise (debug, heap stats, WiFi RSSI)
- ✅ Clear, readable output

---

### ✅ Solution 2: Use Filtered Serial Monitor
**Best for: Clean continuous recording**

#### Run:
```powershell
python serial_monitor_demo.py
```

#### Features:
- Auto-filters debug/noise logs
- Color-coded by importance
- Slower output (500ms delay between logs)
- Highlights: CONFIG, COMMAND, FOTA, SECURITY

#### Benefits:
- ✅ No manual control needed
- ✅ Cleaner output automatically
- ✅ Better for screen recording

---

### ✅ Solution 3: Increase Polling Interval
**Best for: Reduce data acquisition frequency**

#### Before recording, update config:
```json
"acquisition": {
  "polling_interval_ms": 15000  // Was 5000, now 15 seconds
}
```

#### Or send config update via dashboard:
- Set sampling interval to **15** seconds
- This gives you 15 seconds between each data sample
- More time to narrate between events

---

### ✅ Solution 4: Use PlatformIO Monitor with Filter
**Best for: Built-in solution**

#### Run with filter:
```powershell
pio device monitor --baud 115200 --filter send_on_enter
```

#### Or create platformio.ini monitor filter:
```ini
[env:esp32dev]
monitor_filters = 
    colorize
    time
    log2file
```

---

## 🎥 Recommended Workflow for Video

### Pre-Recording Setup:

1. **Install Python packages:**
   ```powershell
   pip install pyserial colorama
   ```

2. **Use demo config with slower polling:**
   - Copy `config/config_demo.json` to `config/config.json`
   - Or update `polling_interval_ms` to `15000`

3. **Start interactive monitor:**
   ```powershell
   python serial_monitor_interactive.py
   ```

### During Recording:

1. **Start with paused monitor:**
   - Press SPACE to pause
   - Introduce the demo
   - Press SPACE to resume

2. **For configuration update demo:**
   - Resume monitor
   - Send config update via dashboard
   - Watch logs appear (color-coded cyan)
   - Press SPACE to pause after "Applied successfully"
   - Narrate what happened
   - Resume to continue

3. **For command execution:**
   - Resume monitor
   - Send command via dashboard
   - Watch execution logs (cyan)
   - Pause when "Command executed successfully" appears
   - Narrate the round-trip
   - Resume

4. **For FOTA demo:**
   - Keep running (don't pause during download)
   - Chunk progress will show clearly
   - Pause after "Firmware verified" or "Hash mismatch"
   - Explain the result
   - Resume for reboot/rollback

### Post-Recording:

- Logs are already color-coded in terminal
- Video will show clear, readable output
- Important events are highlighted automatically

---

## 📊 Log Color Coding

| Color | Meaning | Examples |
|-------|---------|----------|
| 🔴 Red | Errors/Failures | ERROR, FAILED, REJECT |
| 🟢 Green | Success | SUCCESS, VERIFIED, ✓, APPLIED |
| 🔵 Cyan | Important Actions | [CONFIG], [COMMAND], [FOTA] |
| 🟣 Magenta | Security Events | [SECURITY], HMAC, Nonce |
| 🟡 Yellow | Warnings | WARN, ROLLBACK, RETRY |
| ⚪ White | Info | [INFO], general logs |

---

## 🔧 Troubleshooting

### "Port COM5 is busy"
```powershell
# Close any open serial monitors
# Then run:
python serial_monitor_interactive.py
```

### "Module not found: serial"
```powershell
pip install pyserial
```

### "Module not found: colorama"
```powershell
pip install colorama
```

### Logs still too fast
1. Increase `polling_interval_ms` to **20000** (20 seconds)
2. Press SPACE to pause frequently
3. Use `serial_monitor_demo.py` with built-in delay

### Colors not showing
- Run in Windows Terminal or PowerShell
- NOT in old CMD.exe
- Colorama handles Windows compatibility

---

## ⚡ Quick Start Command

```powershell
# One-time setup
pip install pyserial colorama

# For each recording session
python serial_monitor_interactive.py

# Then during video:
# - SPACE to pause/resume
# - F to toggle filter
# - Q to quit
```

---

## 📝 Example Session

```
✓ Connected to COM5

[INFO] EcoWatt Device v1.0.0
[CONFIG] Current: interval=5000ms, registers=[0,1]     ← CYAN

--- Press SPACE to pause ---

[CONFIG] Update received from cloud                    ← CYAN
[CONFIG] New interval: 10000ms                         ← CYAN
[CONFIG] Validation: PASSED                            ← GREEN
[CONFIG] Applied successfully                          ← GREEN

--- Press SPACE to pause and narrate ---

[INFO] Sending acknowledgment to cloud
[DATA] Sample: Reg0=230.0V, Reg1=2.5A, Reg8=75.0%

--- Continue with demo ---
```

---

## 🎯 Best Practices

1. **Always use interactive monitor** during recording
2. **Pause frequently** to narrate without rushed speech
3. **Keep filter ON** to hide noise
4. **Test run first** to see timing
5. **Increase polling interval** if still too fast
6. **Use color coding** to highlight important events visually

---

Good luck with your demo! 🎬✨
