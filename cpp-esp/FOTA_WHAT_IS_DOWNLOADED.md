# What is Downloaded as a FOTA Update?

## **Short Answer: Complete Firmware Binary** 🎯

The FOTA update downloads the **entire compiled firmware** as a binary file (`.bin`), not individual source code files.

---

## What is the Firmware Binary?

### **The `.bin` File Contains:**

```
firmware.bin
├─ Compiled C++ code (all .cpp files)
├─ Compiled libraries (ArduinoJson, Crypto, WiFi, etc.)
├─ Arduino framework code
├─ ESP32 system libraries
├─ Data sections (constants, strings)
└─ Program instructions (machine code)

Result: ONE SINGLE BINARY FILE
Size: ~1 MB (1,068,032 bytes)
```

### **NOT Included in FOTA:**
❌ Source code (.cpp, .hpp files)  
❌ Configuration files (config.json)  
❌ LittleFS filesystem data  
❌ Individual libraries  
❌ Documentation or text files  

✅ **Only the compiled, executable firmware binary!**

---

## Build Process → FOTA Update

### **1. Development Phase (Your Code)**

```
Your Project:
├─ src/
│   ├─ main.ino              ← Your main code
│   ├─ ecoWatt_device.cpp    ← Device logic
│   ├─ fota_manager.cpp      ← FOTA implementation
│   ├─ http_client.cpp       ← HTTP client
│   └─ ... (all other .cpp files)
├─ include/
│   └─ ... (all .hpp files)
├─ lib_deps/
│   ├─ ArduinoJson/          ← JSON library
│   ├─ Crypto/               ← Encryption library
│   └─ ... (other libraries)
└─ platformio.ini            ← Build configuration
```

### **2. Compilation (PlatformIO Build)**

```bash
pio run
```

**What happens:**
```
1. Preprocessor
   ├─ Resolves #include statements
   ├─ Expands #define macros
   └─ Conditional compilation (#ifdef)

2. Compiler (GCC for ESP32)
   ├─ Compiles each .cpp → .o (object file)
   ├─ Optimizes code (-O2 optimization level)
   └─ Generates machine code for ESP32

3. Linker
   ├─ Combines all .o files
   ├─ Links libraries (ArduinoJson, Crypto, WiFi)
   ├─ Resolves function calls
   └─ Creates firmware.elf

4. Binary Generator (esptool.py)
   ├─ Converts .elf → .bin
   ├─ Adds ESP32 boot header
   ├─ Creates flash-ready binary
   └─ Output: firmware.bin
```

**Result:**
```
.pio/build/esp32dev/firmware.bin
Size: 1,068,032 bytes (1.04 MB)
```

### **3. FOTA Upload**

```
firmware.bin (1.04 MB)
     ↓
Split into chunks (1 KB each)
     ↓
Chunk 0: bytes 0-1023
Chunk 1: bytes 1024-2047
Chunk 2: bytes 2048-3071
...
Chunk 1042: bytes 1067008-1068031
     ↓
Upload to cloud server
```

---

## Firmware Binary Structure

### **Inside firmware.bin:**

```
┌────────────────────────────────────────────────────┐
│ Offset    │ Content                                │
├────────────────────────────────────────────────────┤
│ 0x0000    │ ESP32 Image Header                     │
│           │ - Magic number (0xE9)                  │
│           │ - Chip type (ESP32)                    │
│           │ - Flash mode, size, speed              │
├────────────────────────────────────────────────────┤
│ 0x0018    │ Segment Headers                        │
│           │ - Number of segments                   │
│           │ - Load addresses                       │
├────────────────────────────────────────────────────┤
│ 0x1000    │ Code Segment                           │
│           │ - Compiled C++ code (machine code)     │
│           │ - Function implementations:            │
│           │   • setup()                            │
│           │   • loop()                             │
│           │   • ecoWatt_device.cpp functions       │
│           │   • fota_manager.cpp functions         │
│           │   • All other .cpp code                │
├────────────────────────────────────────────────────┤
│ 0x80000   │ Data Segment                           │
│           │ - String constants                     │
│           │ - Global variables                     │
│           │ - Static data                          │
├────────────────────────────────────────────────────┤
│ 0xA0000   │ Read-Only Data                         │
│           │ - const variables                      │
│           │ - Lookup tables                        │
├────────────────────────────────────────────────────┤
│ 0xC0000   │ Library Code                           │
│           │ - ArduinoJson (compiled)               │
│           │ - Crypto library (compiled)            │
│           │ - WiFi library (compiled)              │
│           │ - HTTP client library                  │
├────────────────────────────────────────────────────┤
│ 0xF0000   │ Arduino Framework                      │
│           │ - Arduino core functions               │
│           │ - FreeRTOS (ESP32 OS)                  │
│           │ - ESP-IDF libraries                    │
└────────────────────────────────────────────────────┘

Total: 1,068,032 bytes
```

---

## What Changes Between Versions?

### **Example: Version 1.0.0 → 1.0.1**

**If you change:**
```cpp
// Version 1.0.0
void handleCommand() {
    Logger::info("Command received");
    // ... old logic ...
}

// Version 1.0.1
void handleCommand() {
    Logger::info("Command received with security");
    // ... new security logic ...
}
```

**The ENTIRE firmware.bin is rebuilt and replaced:**

```
firmware_v1.0.0.bin (1,068,032 bytes)
     ↓ FOTA update
firmware_v1.0.1.bin (1,068,156 bytes)  ← Slightly different size
```

**Both contain:**
- ✅ Complete compiled code
- ✅ All libraries
- ✅ All functions (changed and unchanged)
- ✅ Arduino framework

**You can't update just one function!** The entire binary is replaced.

---

## FOTA Update Types

### **1. Full Firmware Update (What we're doing)**

```
Type: Complete binary replacement
Size: ~1 MB (full firmware)
Contains: Everything
Process: Replace entire App partition
Time: 2-3 minutes
```

**Pros:**
- ✅ Simple and reliable
- ✅ Complete version control
- ✅ Can change anything

**Cons:**
- ⚠️ Large download size
- ⚠️ Longer download time

### **2. Delta/Differential Update (Not implemented)**

```
Type: Only changed bytes
Size: ~10-100 KB (only differences)
Contains: Binary diff patch
Process: Apply patch to existing firmware
Time: 10-30 seconds
```

**Pros:**
- ✅ Smaller download
- ✅ Faster update

**Cons:**
- ⚠️ More complex
- ⚠️ Risk of corruption
- ⚠️ Requires matching base version

---

## Build Output Analysis

### **From Your Build:**

```
RAM:   [=         ]  14.4% (used 47,116 bytes from 327,680 bytes)
Flash: [========  ]  81.0% (used 1,061,297 bytes from 1,310,720 bytes)
```

**What this means:**

1. **RAM Usage (47 KB)**
   - Global variables
   - Static data
   - Not included in .bin (allocated at runtime)

2. **Flash Usage (1.04 MB)** ← THIS IS THE .bin FILE!
   - Code + data + libraries
   - This is what gets downloaded in FOTA
   - Stored in App partition

---

## Comparison: Source vs Binary

### **Your Source Code:**

```
Project source files:
├─ src/ (20+ .cpp files)       ~150 KB
├─ include/ (20+ .hpp files)   ~50 KB
├─ Libraries source            ~5 MB
└─ Arduino framework           ~50 MB

Total source: ~55 MB
```

### **Compiled Binary:**

```
firmware.bin: 1.04 MB  ← 98% smaller!
```

**Why so much smaller?**
- ✅ Source code → compact machine code
- ✅ Comments removed
- ✅ Whitespace removed
- ✅ Unused code eliminated
- ✅ Optimized by compiler
- ✅ Only linked code included

---

## What Happens During FOTA?

### **Current State (Before FOTA):**

```
ESP32 Flash:
┌─────────────────────────────────┐
│ App0 (Running)                  │
│ ┌─────────────────────────────┐ │
│ │ firmware v1.0.0             │ │ ← Currently executing
│ │ Size: 1,068,032 bytes       │ │
│ └─────────────────────────────┘ │
└─────────────────────────────────┘
┌─────────────────────────────────┐
│ App1 (OTA Target)               │
│ ┌─────────────────────────────┐ │
│ │ [Empty or old version]      │ │
│ └─────────────────────────────┘ │
└─────────────────────────────────┘
```

### **During FOTA Download:**

```
App0: Still running v1.0.0
      ↓ Normal operation continues

App1: Being written with v1.0.1
      [████████████████░░░░░░] 75% complete
      
Your device works normally during download!
```

### **After FOTA Complete:**

```
ESP32 Flash:
┌─────────────────────────────────┐
│ App0 (Old)                      │
│ ┌─────────────────────────────┐ │
│ │ firmware v1.0.0             │ │ ← Backup (for rollback)
│ └─────────────────────────────┘ │
└─────────────────────────────────┘
┌─────────────────────────────────┐
│ App1 (New - Boot Target)        │
│ ┌─────────────────────────────┐ │
│ │ firmware v1.0.1             │ │ ← Will boot from here
│ │ Size: 1,068,156 bytes       │ │
│ └─────────────────────────────┘ │
└─────────────────────────────────┘

Next reboot → Runs from App1 (v1.0.1)
If v1.0.1 crashes → Auto rollback to App0 (v1.0.0)
```

---

## Summary: What is Downloaded?

| Question | Answer |
|----------|--------|
| **What file?** | `firmware.bin` - Complete compiled binary |
| **Size?** | ~1 MB (1,068,032 bytes) |
| **Contains?** | All code, libraries, framework (machine code) |
| **Source code?** | No - only compiled binary |
| **Config files?** | No - firmware only |
| **LittleFS data?** | No - separate filesystem |
| **How delivered?** | Split into 1043 chunks of 1 KB each |
| **Encoding?** | Base64 (for JSON transport) |
| **Verification?** | HMAC-SHA256 per chunk + full SHA-256 |
| **Where written?** | OTA partition (App1) in flash |
| **When active?** | After reboot |

---

## Example: Build and Upload Flow

### **1. Make Code Changes:**

```cpp
// Change version in main.ino
const char* FIRMWARE_VERSION = "1.0.1";  // Changed from 1.0.0
```

### **2. Build Firmware:**

```bash
pio run
```

**Output:**
```
Building firmware...
Linking .pio\build\esp32dev\firmware.elf
Creating .pio\build\esp32dev\firmware.bin
Success! Size: 1,068,156 bytes
```

### **3. Upload to Cloud:**

```bash
.\test_fota_complete.ps1 -FirmwarePath ".pio\build\esp32dev\firmware.bin" -Version "1.0.1"
```

**What happens:**
```
1. Read firmware.bin (1,068,156 bytes)
2. Calculate SHA-256 hash
3. Split into 1044 chunks (1 KB each)
4. Base64 encode each chunk
5. Upload to cloud with HMAC
```

### **4. ESP32 Downloads:**

```
1. ESP32 polls cloud every 60s
2. Sees v1.0.1 available (current: v1.0.0)
3. Downloads all 1044 chunks
4. Writes each chunk to App1 partition
5. Verifies complete firmware hash
6. Reboots
7. Bootloader loads v1.0.1 from App1
8. Reports boot success to cloud
```

---

## Conclusion

**FOTA downloads:** The **complete compiled firmware binary** (`firmware.bin`)  
**Not:** Source code, configs, or individual files  
**Size:** ~1 MB (your entire compiled application)  
**How:** Split into small chunks, downloaded sequentially  
**Where:** Written directly to flash (OTA partition)  
**Result:** Complete firmware replacement on next boot  

Think of it like updating your phone's OS - you download the entire compiled operating system, not the source code! 📱➡️💾
