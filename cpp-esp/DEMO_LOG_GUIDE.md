📡 FOTA DOWNLOAD DEMO LOG GUIDE
=====================================

🎯 **CURRENT STATUS:**
✅ Firmware v1.0.4 (35KB) uploaded and available
✅ ESP32 currently on v1.0.3
✅ Version mismatch will trigger download on next check

🔍 **WHAT TO WATCH FOR IN FLASK SERVER LOGS:**

## PHASE 1: Normal Check Pattern (Every ~23 seconds)
```
[FOTA] EcoWatt001: boot_status - Status: success
[FOTA] EcoWatt001: fota_completed - New firmware booted successfully
[FOTA STATUS] Device EcoWatt001: chunk None, verified=None, boot=success, rollback=False
10.50.126.50 - - [timestamp] "POST /api/inverter/fota/status HTTP/1.1" 200 -
10.50.126.50 - - [timestamp] "GET /api/inverter/fota/manifest HTTP/1.1" 200 -
```

## PHASE 2: Download Detection & Start
```
[FOTA] EcoWatt001: version_check - Current: 1.0.3, Available: 1.0.4
[FOTA] EcoWatt001: download_initiated - Starting firmware download
10.50.126.50 - - [timestamp] "GET /api/inverter/fota/chunk?offset=0&size=1024 HTTP/1.1" 200 -
[FOTA] EcoWatt001: chunk_1 - Downloaded chunk 1/35 (1024 bytes)
```

## PHASE 3: Chunk Download Progress (35 chunks total)
```
10.50.126.50 - - [timestamp] "GET /api/inverter/fota/chunk?offset=1024&size=1024 HTTP/1.1" 200 -
[FOTA] EcoWatt001: chunk_2 - Downloaded chunk 2/35 (1024 bytes)
10.50.126.50 - - [timestamp] "GET /api/inverter/fota/chunk?offset=2048&size=1024 HTTP/1.1" 200 -
[FOTA] EcoWatt001: chunk_3 - Downloaded chunk 3/35 (1024 bytes)
...
10.50.126.50 - - [timestamp] "GET /api/inverter/fota/chunk?offset=34816&size=184 HTTP/1.1" 200 -
[FOTA] EcoWatt001: chunk_35 - Downloaded final chunk (184 bytes)
```

## PHASE 4: Verification & Installation
```
[FOTA] EcoWatt001: download_completed - Total: 35000 bytes received
[FOTA] EcoWatt001: verification_started - Checking SHA256 hash
[FOTA] EcoWatt001: verification_passed - Hash matches: 932ca447d979...
[FOTA] EcoWatt001: installation_started - Writing to flash memory
[FOTA] EcoWatt001: installation_completed - Firmware installed successfully
[FOTA] EcoWatt001: reboot_initiated - Restarting with new firmware
```

## PHASE 5: Post-Reboot Verification
```
[ESP32 REBOOT - Connection temporarily lost]
[FOTA] EcoWatt001: boot_status - Status: success
[FOTA] EcoWatt001: fota_completed - New firmware v1.0.4 booted successfully
[FOTA STATUS] Device EcoWatt001: chunk None, verified=None, boot=success, rollback=False
```

🎥 **DEMO RECORDING TIPS:**

1. **Setup (30 seconds):**
   - Show both terminals side by side
   - Point out the regular 23-second check pattern
   - Mention that v1.0.4 is now available for download

2. **Download Detection (30 seconds):**
   - Watch for the version mismatch detection
   - Highlight when chunk requests start appearing
   - Count the rapid succession of chunk downloads

3. **Progress Tracking (45 seconds):**
   - Point out the chunk URLs with increasing offsets
   - Show the progress: 1/35, 2/35, etc.
   - Mention the 1KB chunk size strategy

4. **Completion (30 seconds):**
   - Show the verification phase
   - Wait for the reboot
   - Confirm new version is running

5. **Success Confirmation (15 seconds):**
   - Show the new boot status with v1.0.4
   - Highlight successful completion

⏰ **TIMING:**
- ESP32 checks every ~23 seconds
- Download takes ~30-60 seconds (35 chunks)
- Total demo: ~3-5 minutes from start to finish

🔧 **TROUBLESHOOTING:**
If no download happens after 2-3 checks:
- ESP32 might already be on v1.0.4
- Upload v1.0.5: `python fota_complete_test.py --mode upload --version 1.0.5 --size 36000`
- Check ESP32 serial monitor for current version

📊 **SUCCESS INDICATORS:**
✅ Rapid succession of chunk download requests
✅ Progress from chunk 1/35 to 35/35
✅ SHA256 verification passed
✅ ESP32 reboot and version change
✅ New status: "New firmware v1.0.4 booted successfully"