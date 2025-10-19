# Milestone 4 - Parts 1 & 2 Demonstration Script
## Remote Configuration & Command Execution Demo

**Duration:** 8-10 minutes  
**Presenter:** [Group member who hasn't presented before]

---

## 🎬 DEMO STRUCTURE

### Introduction (30 seconds)
- **Camera on** - Introduce yourself
- State: "Today I'll demonstrate Part 1: Remote Configuration and Part 2: Command Execution from Milestone 4"
- Brief overview: "I'll show how our EcoWatt Device accepts runtime configuration updates from the cloud and executes commands on the Inverter SIM"

---

## 📋 PART 1: REMOTE CONFIGURATION DEMONSTRATION (4-5 minutes)

### 1. Show Initial System State (1 minute)

**What to show:**
```
✓ Open Serial Monitor showing device startup
✓ Show current configuration being loaded
✓ Point out current sampling frequency (e.g., 5 seconds)
✓ Show which registers are being polled (e.g., voltage, current)
```

**What to say:**
> "First, let me show you the device's current state. As you can see in the serial monitor, the device is polling voltage and current registers every 5 seconds. This is the baseline configuration."

**Screen to show:**
- Serial monitor output showing regular data acquisition
- Example:
```
[CONFIG] Current sampling interval: 5000ms
[ACQ] Polling registers: VOLTAGE, CURRENT
[DATA] V=230.5V, I=12.3A
```

---

### 2. Demonstrate Configuration Update via Cloud Dashboard (2 minutes)

**What to do:**
```
1. Open Node-RED dashboard or your cloud interface
2. Show the configuration update form/interface
3. Explain what you'll change:
   - Change sampling frequency from 5s to 10s
   - Add new register: ACTIVE_POWER
```

**What to say:**
> "Now I'll send a configuration update from the EcoWatt Cloud. I'm changing the sampling interval to 10 seconds and adding Active Power to the register list. Notice that I'm doing this without reflashing or rebooting the device."

**Screen to show:**
- Cloud dashboard with configuration form
- Fill in new values
- Click "Send Configuration Update"

**Configuration JSON example to show:**
```json
{
  "device_id": "ECOWATT_001",
  "sampling_interval_ms": 10000,
  "registers": ["VOLTAGE", "CURRENT", "ACTIVE_POWER"],
  "timestamp": "2025-10-18T10:30:00Z"
}
```

---

### 3. Show Device Receiving and Validating Configuration (1.5 minutes)

**What to show:**
```
✓ Serial monitor showing configuration received
✓ Validation process
✓ Parameters being applied
✓ Confirmation sent back to cloud
```

**What to say:**
> "Watch the serial monitor. The device receives the configuration at the next upload slot, validates each parameter, and applies them at runtime. Notice the acquisition scheduler is now using the new 10-second interval and polling the new register."

**Expected serial output:**
```
[CONFIG] Configuration update received
[CONFIG] Validating parameters...
[CONFIG] ✓ sampling_interval_ms: 10000 (valid)
[CONFIG] ✓ registers: VOLTAGE, CURRENT, ACTIVE_POWER (valid)
[CONFIG] Applying configuration...
[ACQ] Scheduler updated: interval=10000ms
[ACQ] Register list updated: 3 registers
[CONFIG] Configuration applied successfully
[UPLOAD] Sending confirmation to cloud...
```

---

### 4. Show Configuration Persistence (30 seconds)

**What to do:**
```
1. Show a few data acquisitions with new config
2. Optionally: mention that config is saved to non-volatile memory
```

**What to say:**
> "As you can see, the device is now polling every 10 seconds and including Active Power in the readings. This configuration persists across reboots because it's stored in non-volatile memory."

**Screen to show:**
```
[DATA] V=230.2V, I=12.1A, P=2784W
[Wait 10 seconds]
[DATA] V=230.8V, I=12.4A, P=2862W
```

---

### 5. Demonstrate Invalid Configuration Rejection (1 minute)

**What to do:**
```
1. Send an invalid configuration (e.g., sampling interval = 0ms)
2. Show device rejecting it
3. Show error report sent back to cloud
```

**What to say:**
> "Now let me demonstrate validation. I'll send an invalid configuration with a sampling interval of zero. Watch how the device rejects it and reports the error back to the cloud."

**Expected serial output:**
```
[CONFIG] Configuration update received
[CONFIG] Validating parameters...
[CONFIG] ✗ sampling_interval_ms: 0 (INVALID - must be > 1000ms)
[CONFIG] Configuration rejected
[UPLOAD] Sending error report to cloud...
```

**Show cloud dashboard:**
- Display the error log showing which parameter was rejected and why

---

## 🎮 PART 2: COMMAND EXECUTION DEMONSTRATION (3-4 minutes)

### 6. Explain Command Execution Flow (30 seconds)

**What to say:**
> "Part 2 demonstrates command execution. The flow is: Cloud queues a command → Device receives it at next slot → Device forwards to Inverter SIM → SIM executes → Device reports result back to Cloud."

**Optional: Show a flow diagram on screen**

---

### 7. Queue Command from Cloud (1 minute)

**What to do:**
```
1. Open command execution interface on cloud dashboard
2. Select a write command (e.g., write to a control register)
3. Show the command being queued
```

**What to say:**
> "I'll queue a write command to change the inverter's operating mode. In the cloud interface, I'm sending a command to write value 0x02 to register 0x1000, which switches the inverter to standby mode."

**Command JSON example:**
```json
{
  "device_id": "ECOWATT_001",
  "command_type": "WRITE_REGISTER",
  "register_address": "0x1000",
  "value": "0x02",
  "command_id": "CMD_20251018_001",
  "timestamp": "2025-10-18T10:35:00Z"
}
```

**Show cloud dashboard:**
- Command status: "QUEUED"

---

### 8. Device Receives and Executes Command (1.5 minutes)

**What to show:**
```
✓ Device receives command at next transmission slot
✓ Device parses and validates command
✓ Device forwards to Inverter SIM
✓ Show Inverter SIM API call/response (optional)
✓ Device receives execution result
```

**What to say:**
> "At the next upload interval, the device receives the queued command. It validates the command, then forwards it to the Inverter SIM. The SIM executes the write operation and returns a status code."

**Expected serial output:**
```
[UPLOAD] Checking for pending commands...
[CMD] Command received: CMD_20251018_001
[CMD] Type: WRITE_REGISTER, Address: 0x1000, Value: 0x02
[CMD] Validating command...
[CMD] ✓ Command valid
[CMD] Forwarding to Inverter SIM...
[SIM] Writing register 0x1000 = 0x02
[SIM] Response: SUCCESS (status=0x00)
[CMD] Command executed successfully
[CMD] Caching result for next upload...
```

---

### 9. Device Reports Result Back to Cloud (1 minute)

**What to show:**
```
✓ At the following transmission slot, device sends result
✓ Cloud receives and logs the result
✓ Show command status updated in cloud dashboard
```

**What to say:**
> "At the next upload slot, the device reports the execution result back to the cloud. The cloud logs show the complete command lifecycle: queued, executed, and confirmed."

**Expected serial output:**
```
[UPLOAD] Sending command results...
[CMD] Reporting: CMD_20251018_001 = SUCCESS
[UPLOAD] Command results uploaded
```

**Show cloud dashboard:**
- Command log showing:
  - Command ID: CMD_20251018_001
  - Status: COMPLETED
  - Result: SUCCESS
  - Execution timestamp
  - Full audit trail

---

### 10. Demonstrate Command Failure Handling (Optional, 30 seconds)

**What to do:**
```
1. Send a command that will fail (invalid register or value)
2. Show error being reported back
```

**What to say:**
> "Finally, let me show error handling. I'll send a command to an invalid register. Watch how the device reports the failure back to the cloud."

**Expected output:**
```
[CMD] Command received: CMD_20251018_002
[CMD] Forwarding to Inverter SIM...
[SIM] Response: ERROR (status=0x02 - Invalid Register)
[CMD] Command failed: Invalid Register
```

---

## 🎯 CLOSING (30 seconds)

### Summary

**What to say:**
> "To summarize, I've demonstrated:
> 1. Remote configuration updates applied at runtime without reboot
> 2. Configuration validation and error reporting
> 3. Configuration persistence in non-volatile memory
> 4. Complete command execution flow from cloud to device to SIM and back
> 5. Command logging and audit trail on both device and cloud
> 
> All of these features work together seamlessly while the device continues its normal operation. Thank you."

---

## 📝 CHECKLIST BEFORE RECORDING

### Preparation
- [ ] Device firmware uploaded and running
- [ ] Flask server / Node-RED dashboard running
- [ ] Serial monitor open and visible
- [ ] Cloud dashboard open in browser
- [ ] Test configuration updates beforehand
- [ ] Test command execution beforehand
- [ ] Clear any old logs for clean demo
- [ ] Good lighting and clear audio
- [ ] Screen recording software ready

### During Recording
- [ ] Camera on throughout
- [ ] Speak clearly and confidently
- [ ] Point to relevant parts of screen
- [ ] Explain what's happening in real-time
- [ ] Show both serial monitor and cloud dashboard
- [ ] Demonstrate success AND failure cases
- [ ] Keep within 10-minute time limit

### Files to Show (briefly in code walkthrough)
- [ ] `config_manager.hpp/cpp` - Configuration handling
- [ ] `command_executor.hpp/cpp` - Command execution
- [ ] `app.py` or Node-RED flows - Cloud API
- [ ] Configuration storage location

---

## 🔧 TECHNICAL SETUP COMMANDS

### Before Demo:
```powershell
# Start Flask server
python app.py

# Open serial monitor
pio device monitor

# Upload latest firmware
pio run --target upload --upload-port COM5
```

### During Demo - Windows to Show:
1. Serial Monitor (PlatformIO or Arduino IDE)
2. Cloud Dashboard (Browser - Node-RED or Flask UI)
3. Code Editor (optional - for quick code walkthrough)
4. Command Line (if showing API calls directly)

---

## 📊 KEY POINTS TO EMPHASIZE

### Remote Configuration (Part 1)
✓ **Runtime updates** - No reboot required  
✓ **Thread-safe** - Doesn't interrupt acquisition  
✓ **Validation** - Rejects invalid parameters  
✓ **Idempotent** - Handles duplicates gracefully  
✓ **Persistent** - Survives power cycles  
✓ **Bidirectional** - Device confirms to cloud  

### Command Execution (Part 2)
✓ **Asynchronous** - Commands queued and executed at slots  
✓ **Reliable** - Status reported back to cloud  
✓ **Logged** - Complete audit trail maintained  
✓ **Error handling** - Failures reported properly  
✓ **Non-blocking** - Doesn't disrupt normal operation  

---

## 🎥 VIDEO STRUCTURE TIMELINE

| Time | Section | Content |
|------|---------|---------|
| 0:00-0:30 | Intro | Welcome, overview |
| 0:30-1:30 | Initial State | Show current config, acquisition |
| 1:30-3:30 | Config Update | Send update, show validation, apply |
| 3:30-4:30 | Config Validation | Show rejection of invalid config |
| 4:30-5:00 | Flow Explanation | Explain command execution flow |
| 5:00-6:00 | Queue Command | Show cloud interface, queue command |
| 6:00-7:30 | Execute Command | Device receives, forwards, gets result |
| 7:30-8:30 | Report Result | Device reports back, show logs |
| 8:30-9:00 | Error Handling | Optional failure case |
| 9:00-9:30 | Closing | Summary and thank you |

---

## 💡 TIPS FOR GREAT DEMO

1. **Practice first** - Do a complete dry run
2. **Speak naturally** - Don't just read the script
3. **Show enthusiasm** - Be confident in your work
4. **Zoom in** - Make text readable in recording
5. **Use pointers** - Highlight what you're talking about
6. **Pause briefly** - Let viewers absorb information
7. **Handle errors gracefully** - If something fails, explain why
8. **Time management** - Keep each section within time limits

---

## 🚨 TROUBLESHOOTING

### If device doesn't receive config:
- Check if upload cycle is working
- Verify cloud API endpoint
- Check network connectivity

### If command doesn't execute:
- Verify command format matches expected schema
- Check Inverter SIM is responding
- Review command validation logic

### If demo runs long:
- Skip optional failure demonstration
- Reduce time on code walkthrough
- Focus on live demo over explanation

---

**Good luck with your demonstration!** 🎬✨
