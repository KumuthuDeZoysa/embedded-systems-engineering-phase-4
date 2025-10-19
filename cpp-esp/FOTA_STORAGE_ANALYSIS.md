# FOTA Storage Analysis - ESP32

## ESP32 Storage Specifications

### Flash Memory
- **Total Flash:** 4 MB (4,194,304 bytes)
- **RAM:** 320 KB (327,680 bytes)

### OTA Partition Layout (4MB Flash)
```
┌─────────────────────────────────────────┐
│ Bootloader         │ 64 KB              │
├─────────────────────────────────────────┤
│ Partition Table    │ 12 KB              │
├─────────────────────────────────────────┤
│ NVS                │ 20 KB              │
├─────────────────────────────────────────┤
│ OTA Data           │ 8 KB               │
├─────────────────────────────────────────┤
│ App0 (factory)     │ ~1.3 MB (1,310,720)│ ← Current firmware
├─────────────────────────────────────────┤
│ App1 (OTA)         │ ~1.3 MB (1,310,720)│ ← New firmware downloaded here
├─────────────────────────────────────────┤
│ SPIFFS/LittleFS    │ ~1.5 MB            │
└─────────────────────────────────────────┘
```

## Current Firmware Analysis

### Full Firmware (`.pio\build\esp32dev\firmware.bin`)
- **Size:** 1,068,032 bytes (1.043 MB)
- **Chunks (1KB):** 1043 chunks
- **Partition usage:** 81.5%
- **Free space in OTA partition:** 242,688 bytes (~237 KB)
- ✅ **Fits comfortably in OTA partition**

### Test Firmware (`test_firmware_10kb.bin`)
- **Size:** 10,240 bytes (10 KB)
- **Chunks (1KB):** 10 chunks
- **Purpose:** Fast testing without long download times

## Memory Usage During FOTA

### Per-Chunk Processing
```
┌────────────────────────────────────────────────┐
│ Chunk Processing (ONE chunk at a time)        │
├────────────────────────────────────────────────┤
│ 1. HTTP GET request                            │
│    → Response: ~1366 bytes (Base64 encoded)    │
│                                                │
│ 2. JSON parsing                                │
│    → Buffer: ~200 bytes                        │
│                                                │
│ 3. Base64 decode                               │
│    → Input: ~1366 bytes (Base64)               │
│    → Output: 1024 bytes (binary)               │
│                                                │
│ 4. HMAC verification                           │
│    → Buffer: ~64 bytes (SHA-256 hash)          │
│                                                │
│ 5. Write to OTA partition                      │
│    → Direct flash write (no extra RAM)         │
│                                                │
│ Total RAM per chunk: ~2-3 KB                   │
│ Peak RAM usage: ~30-50 KB (including stack)    │
└────────────────────────────────────────────────┘
```

### RAM is NOT a Problem Because:
✅ **Chunks are processed ONE AT A TIME**  
✅ **Each chunk is immediately written to flash**  
✅ **Previous chunk data is discarded before downloading next**  
✅ **Only ~3KB RAM needed per chunk**  
✅ **ESP32 has 320KB RAM available**

## Download Process

### Step-by-Step Flow
```
1. ESP32 polls cloud every 60s
   └─ GET /api/inverter/fota/manifest
   
2. If new firmware available:
   └─ Check version number
   └─ Get total chunks count
   
3. For each chunk (0 to N-1):
   ├─ GET /api/inverter/fota/chunk?chunk_number=X
   ├─ Receive JSON: {"chunk_number": X, "data": "base64...", "hmac": "..."}
   ├─ Decode Base64 → 1024 bytes binary
   ├─ Verify HMAC-SHA256
   ├─ Write to OTA partition
   └─ Report status to cloud
   
4. After all chunks downloaded:
   ├─ Verify complete firmware SHA-256
   ├─ Call esp_ota_set_boot_partition()
   └─ Reboot to new firmware
   
5. After reboot:
   ├─ New firmware boots from App1 partition
   ├─ Report boot status to cloud
   └─ If boot fails → automatic rollback to App0
```

## Storage Constraints

### Maximum Firmware Size
- **Hard limit:** 1,310,720 bytes (~1.28 MB)
- **Current firmware:** 1,068,032 bytes (81.5% usage)
- **Recommended max:** 1,200,000 bytes (91.5% usage)
- **Safety margin:** Keep at least 100KB free for metadata

### Why the Limit?
The ESP32 uses **dual OTA partitions** (App0 + App1) to enable safe firmware updates:
- **App0:** Currently running firmware
- **App1:** New firmware download destination
- After successful download and verification → switch to App1
- If App1 boot fails → automatically rollback to App0

Both partitions must be same size (~1.3MB each), which limits maximum firmware size.

## Network Transfer Analysis

### Full Firmware (1043 KB)
```
Raw binary:        1,068,032 bytes
Base64 encoded:    1,424,043 bytes (+33% overhead)
Network transfer:  ~1.36 MB

At 100 KB/s:       ~14 seconds
At 50 KB/s:        ~28 seconds  
At 10 KB/s:        ~140 seconds (2.3 minutes)

Processing time:   ~10 seconds (HMAC verification)
Total time:        ~2-3 minutes (typical WiFi)
```

### Test Firmware (10 KB)
```
Raw binary:        10,240 bytes
Base64 encoded:    13,654 bytes
Network transfer:  ~13 KB

Download time:     <1 second
Processing time:   ~1 second
Total time:        ~10-15 seconds (10 chunks × 1s each)
```

## Recommendations

### For Development/Testing
✅ **Use test_firmware_10kb.bin (10 chunks)**
- Fast iteration (~15 seconds total)
- Tests all FOTA features
- Saves development time

### For Production
✅ **Keep firmware under 1.2 MB**
- Current: 1.04 MB ✓
- Room for growth: ~150 KB
- Monitor partition usage in build logs

### Optimization Tips
1. **Enable compiler optimizations** (already using `-O2`)
2. **Remove debug symbols** in production
3. **Use compression** for large data assets
4. **Split features** into optional modules if needed

## Build Output Reference

From `platformio.ini`:
```ini
Checking size .pio\build\esp32dev\firmware.elf
RAM:   [=         ]  14.4% (used 47116 bytes from 327680 bytes)
Flash: [========  ]  81.0% (used 1061297 bytes from 1310720 bytes)
                                    ^^^^^^^^^^^    ^^^^^^^^^^^
                                    Current Size   Max Size
```

**Flash usage: 81.0%** ← Healthy! Keep under 90%

## Summary

| Metric | Value | Status |
|--------|-------|--------|
| ESP32 Total Flash | 4 MB | ✅ |
| OTA Partition Size | 1.31 MB | ✅ |
| Current Firmware | 1.04 MB | ✅ 81% |
| Chunks (1KB each) | 1043 | ✅ |
| RAM per chunk | 3 KB | ✅ |
| Total RAM needed | 30-50 KB | ✅ |
| Download time (WiFi) | 2-3 min | ✅ |
| Test firmware | 10 KB / 10 chunks | ✅ Fast! |

**Conclusion:** Current FOTA implementation is well-designed and efficiently uses ESP32's storage. Chunks are processed one-at-a-time, keeping RAM usage minimal while the full firmware fits comfortably in the OTA partition with room to grow.
