# Milestone 4 Part 3 - Security Layer Testing Guide

## Overview

This guide provides **step-by-step instructions** for testing the security implementation. Follow these tests in order to verify that all security features work correctly.

---

## Prerequisites

Before testing, ensure:
1. ✅ Security layer is integrated (follow `SECURITY_INTEGRATION_GUIDE.md`)
2. ✅ Config updated with PSK and security settings
3. ✅ Device can connect to WiFi
4. ✅ Cloud server is running with security enabled
5. ✅ Serial monitor available for debugging

---

## Test Setup

### 1. Generate PSK (Pre-Shared Key)

Generate a 256-bit (32 byte) PSK for testing:

```bash
# Using OpenSSL (recommended)
openssl rand -hex 32
```

**Example output**: `3f7a9c2b8d1e6f0a5c4b3e2d7f9a1c8b6d5e4f3a2b1c0d9e8f7a6b5c4d3e2f1a0`

### 2. Update Configuration

Update `config/config.json`:

```json
{
  "mqtt": { ... },
  "wifi": { ... },
  "modbus": { ... },
  "security": {
    "enabled": true,
    "psk": "3f7a9c2b8d1e6f0a5c4b3e2d7f9a1c8b6d5e4f3a2b1c0d9e8f7a6b5c4d3e2f1a0",
    "use_real_encryption": false,
    "strict_nonce_checking": true,
    "nonce_window": 100
  }
}
```

### 3. Update Cloud Server

Update `cloud_server.py` with the same PSK:

```python
# At top of file
SECURITY_ENABLED = True
PSK = bytes.fromhex("3f7a9c2b8d1e6f0a5c4b3e2d7f9a1c8b6d5e4f3a2b1c0d9e8f7a6b5c4d3e2f1a0")
```

---

## Phase 1: Basic Functionality Tests

### Test 1.1: Security Layer Initialization

**Objective**: Verify security layer initializes correctly

**Steps**:
1. Upload firmware to device
2. Open serial monitor
3. Reboot device

**Expected Serial Output**:
```
[Security] Initializing security layer...
[Security] PSK loaded (64 hex chars)
[Security] Loading nonce state from LittleFS...
[Security] Nonce state loaded: last_nonce=0
[Security] Security layer initialized successfully
```

**Pass Criteria**: ✅ No errors, security initialized

**If Failed**:
- Check PSK is 64 hex characters in config.json
- Verify LittleFS is formatted correctly
- Check serial for specific error messages

---

### Test 1.2: HMAC Computation

**Objective**: Verify HMAC-SHA256 is working

**Steps**:
1. Add test code to `setup()`:
```cpp
void setup() {
    // ... existing setup ...
    
    // Test HMAC
    String testData = "Hello World";
    String mac = securityLayer->computeHMAC(testData.c_str(), config.security.psk.c_str());
    
    Serial.println("[TEST] HMAC Test:");
    Serial.println("  Data: " + testData);
    Serial.println("  MAC: " + mac);
    Serial.println("  Length: " + String(mac.length()));
}
```

**Expected Output**:
```
[TEST] HMAC Test:
  Data: Hello World
  MAC: a591a6d40bf420404a011733cfb7b190d62c65bf0bcda32b57b277d9ad9f146e
  Length: 64
```

**Pass Criteria**: 
- ✅ MAC is 64 hex characters
- ✅ MAC is consistent (same input = same output)
- ✅ MAC changes if input changes

---

### Test 1.3: Nonce Generation

**Objective**: Verify nonces are sequential and persistent

**Steps**:
1. Add test code:
```cpp
void setup() {
    // ... existing setup ...
    
    Serial.println("[TEST] Nonce Generation:");
    for(int i = 0; i < 5; i++) {
        uint32_t nonce = securityLayer->getNextNonce();
        Serial.println("  Nonce " + String(i+1) + ": " + String(nonce));
    }
}
```

2. Upload and check serial
3. Reboot device
4. Check serial again

**Expected Output (First Boot)**:
```
[TEST] Nonce Generation:
  Nonce 1: 1
  Nonce 2: 2
  Nonce 3: 3
  Nonce 4: 4
  Nonce 5: 5
```

**Expected Output (After Reboot)**:
```
[TEST] Nonce Generation:
  Nonce 1: 6
  Nonce 2: 7
  Nonce 3: 8
  ...
```

**Pass Criteria**: 
- ✅ Nonces increment sequentially
- ✅ Nonces persist across reboots
- ✅ No duplicate nonces

---

## Phase 2: Message Security Tests

### Test 2.1: Secure Message Creation

**Objective**: Verify device can create secured messages

**Steps**:
1. Add test code:
```cpp
void setup() {
    // ... existing setup ...
    
    String plainJson = "{\"test\":\"data\",\"value\":123}";
    SecuredMessage secured;
    SecurityResult result = securityLayer->secureMessage(plainJson.c_str(), secured);
    
    Serial.println("[TEST] Secure Message:");
    Serial.println("  Success: " + String(result.is_success()));
    Serial.println("  Nonce: " + String(secured.nonce));
    Serial.println("  Timestamp: " + String(secured.timestamp));
    Serial.println("  Encrypted: " + String(secured.encrypted));
    Serial.println("  Payload length: " + String(secured.payload.length()));
    Serial.println("  MAC: " + secured.mac.c_str());
    
    String envelope = securityLayer->generateSecuredEnvelope(secured);
    Serial.println("  Envelope: " + envelope);
}
```

**Expected Output**:
```
[TEST] Secure Message:
  Success: 1
  Nonce: 1
  Timestamp: 1729012345678
  Encrypted: 1
  Payload length: 44
  MAC: f2b3a4...
  Envelope: {"nonce":1,"timestamp":1729012345678,...}
```

**Pass Criteria**: 
- ✅ Result is success
- ✅ Nonce > 0
- ✅ MAC is 64 characters
- ✅ Envelope is valid JSON

---

### Test 2.2: Message Verification (Round-trip)

**Objective**: Verify device can verify its own messages

**Steps**:
```cpp
void setup() {
    // ... existing setup ...
    
    // Create and verify message
    String original = "{\"test\":\"round-trip\"}";
    SecuredMessage secured;
    securityLayer->secureMessage(original.c_str(), secured);
    
    String envelope = securityLayer->generateSecuredEnvelope(secured);
    
    String recovered;
    SecurityResult result = securityLayer->verifyMessage(envelope.c_str(), recovered);
    
    Serial.println("[TEST] Round-trip:");
    Serial.println("  Original: " + original);
    Serial.println("  Recovered: " + recovered);
    Serial.println("  Match: " + String(original == recovered));
    Serial.println("  Status: " + String((int)result.status));
}
```

**Expected Output**:
```
[TEST] Round-trip:
  Original: {"test":"round-trip"}
  Recovered: {"test":"round-trip"}
  Match: 1
  Status: 0
```

**Pass Criteria**: 
- ✅ Original matches recovered
- ✅ Status is OK (0)

---

## Phase 3: Security Attack Tests

### Test 3.1: Replay Attack Detection

**Objective**: Verify device rejects replayed messages

**Steps**:
```cpp
void setup() {
    // ... existing setup ...
    
    // Create message
    String plain = "{\"test\":\"replay\"}";
    SecuredMessage secured;
    securityLayer->secureMessage(plain.c_str(), secured);
    String envelope = securityLayer->generateSecuredEnvelope(secured);
    
    // First verification (should succeed)
    String recovered1;
    SecurityResult result1 = securityLayer->verifyMessage(envelope.c_str(), recovered1);
    Serial.println("[TEST] Replay Attack:");
    Serial.println("  First attempt: " + String(result1.is_success()));
    
    // Second verification (should fail - replay)
    String recovered2;
    SecurityResult result2 = securityLayer->verifyMessage(envelope.c_str(), recovered2);
    Serial.println("  Second attempt: " + String(result2.is_success()));
    Serial.println("  Status: " + String((int)result2.status));
}
```

**Expected Output**:
```
[TEST] Replay Attack:
  First attempt: 1
  Second attempt: 0
  Status: 3
```

**Pass Criteria**: 
- ✅ First attempt succeeds
- ✅ Second attempt fails
- ✅ Status is REPLAY_DETECTED (3)

---

### Test 3.2: MAC Tampering Detection

**Objective**: Verify device rejects tampered messages

**Steps**:
```cpp
void setup() {
    // ... existing setup ...
    
    // Create message
    String plain = "{\"amount\":100}";
    SecuredMessage secured;
    securityLayer->secureMessage(plain.c_str(), secured);
    
    // Tamper with payload
    secured.payload = "eyJ0YW1wZXJlZCI6dHJ1ZX0=";  // Modified Base64
    
    String envelope = securityLayer->generateSecuredEnvelope(secured);
    String recovered;
    SecurityResult result = securityLayer->verifyMessage(envelope.c_str(), recovered);
    
    Serial.println("[TEST] MAC Tampering:");
    Serial.println("  Success: " + String(result.is_success()));
    Serial.println("  Status: " + String((int)result.status));
}
```

**Expected Output**:
```
[TEST] MAC Tampering:
  Success: 0
  Status: 2
```

**Pass Criteria**: 
- ✅ Verification fails
- ✅ Status is INVALID_MAC (2)

---

### Test 3.3: Old Nonce Rejection

**Objective**: Verify device rejects old nonces (out-of-window)

**Steps**:
```cpp
void setup() {
    // ... existing setup ...
    
    // Generate some nonces to advance counter
    for(int i = 0; i < 150; i++) {
        securityLayer->getNextNonce();
    }
    
    // Try to verify message with very old nonce
    Serial.println("[TEST] Old Nonce:");
    Serial.println("  Current nonce: 150");
    Serial.println("  Old nonce valid (10): " + String(securityLayer->isNonceValid(10)));
    Serial.println("  Recent nonce valid (145): " + String(securityLayer->isNonceValid(145)));
}
```

**Expected Output** (with nonce_window=100):
```
[TEST] Old Nonce:
  Current nonce: 150
  Old nonce valid (10): 0
  Recent nonce valid (145): 1
```

**Pass Criteria**: 
- ✅ Nonce 10 rejected (too old)
- ✅ Nonce 145 accepted (within window)

---

## Phase 4: Integration Tests

### Test 4.1: Secured Data Upload

**Objective**: Verify data upload uses security layer

**Steps**:
1. Ensure device is connected to WiFi
2. Enable verbose logging in uplink_packetizer.cpp
3. Trigger a data upload (wait for scheduled upload or trigger manually)
4. Check serial output

**Expected Output**:
```
[Uplink] Preparing upload...
[Security] Securing message with nonce 5...
[Security] Payload encrypted
[Security] HMAC computed: a3f2b1...
[Uplink] Secured payload prepared
[HTTP] POST /api/inverter/readings
[HTTP] Response: 200 OK
[Uplink] Upload successful
```

**Server Side** (check cloud_server.py logs):
```
INFO: Received secured message
INFO: Nonce: 5
INFO: HMAC verification: PASS
INFO: Payload decoded successfully
```

**Pass Criteria**: 
- ✅ Device secures payload before upload
- ✅ Server verifies HMAC successfully
- ✅ Upload completes with 200 OK

---

### Test 4.2: Secured Config Request/Response

**Objective**: Verify remote config uses security

**Steps**:
1. Trigger config request (wait for polling or trigger manually)
2. Check serial output
3. Verify config is applied

**Expected Output (Device)**:
```
[RemoteCfg] Polling for config...
[HTTP] GET /api/inverter/config
[HTTP] Response received
[Security] Verifying response...
[Security] Nonce: 100
[Security] HMAC verification: PASS
[RemoteCfg] Config verified and parsed
[RemoteCfg] Applying: sampling_interval=10
```

**Expected Output (Server)**:
```
INFO: Config request received
INFO: Securing response with nonce 100
INFO: HMAC computed: d4e5f6...
INFO: Config response sent
```

**Pass Criteria**: 
- ✅ Device verifies server response
- ✅ HMAC verification passes
- ✅ Config is applied correctly

---

### Test 4.3: Secured Command Execution

**Objective**: Verify command execution uses security

**Steps**:
1. Send command from cloud server
2. Check device serial output
3. Verify command executed

**Expected Output**:
```
[RemoteCfg] Command received
[Security] Verifying command message...
[Security] Nonce: 200
[Security] HMAC verification: PASS
[Security] Payload decrypted
[CommandExec] Executing command: write_register
[CommandExec] Target: 40001
[CommandExec] Value: 500
[Modbus] Write successful
[CommandExec] Command completed
```

**Pass Criteria**: 
- ✅ Command message verified
- ✅ Command executed successfully
- ✅ No security errors

---

## Phase 5: Performance Tests

### Test 5.1: Security Overhead

**Objective**: Measure performance impact of security

**Steps**:
```cpp
void setup() {
    // ... existing setup ...
    
    String payload = "{\"test\":\"performance\"}";
    
    // Test without security
    unsigned long start1 = millis();
    for(int i = 0; i < 10; i++) {
        // Simulate plain operation
    }
    unsigned long time1 = millis() - start1;
    
    // Test with security
    unsigned long start2 = millis();
    for(int i = 0; i < 10; i++) {
        SecuredMessage secured;
        securityLayer->secureMessage(payload.c_str(), secured);
    }
    unsigned long time2 = millis() - start2;
    
    Serial.println("[TEST] Performance:");
    Serial.println("  Plain (10x): " + String(time1) + " ms");
    Serial.println("  Secured (10x): " + String(time2) + " ms");
    Serial.println("  Overhead: " + String(time2 - time1) + " ms");
    Serial.println("  Per operation: " + String((time2 - time1) / 10.0) + " ms");
}
```

**Expected Output**:
```
[TEST] Performance:
  Plain (10x): 5 ms
  Secured (10x): 25 ms
  Overhead: 20 ms
  Per operation: 2.0 ms
```

**Pass Criteria**: 
- ✅ Overhead < 5 ms per operation (Base64 mode)
- ✅ Overhead < 15 ms per operation (AES mode)

---

### Test 5.2: Memory Usage

**Objective**: Check heap fragmentation and usage

**Steps**:
```cpp
void setup() {
    // ... existing setup ...
    
    Serial.println("[TEST] Memory:");
    Serial.println("  Free heap: " + String(ESP.getFreeHeap()));
    
    // Allocate security objects
    for(int i = 0; i < 5; i++) {
        SecuredMessage msg;
        securityLayer->secureMessage("{\"test\":123}", msg);
    }
    
    Serial.println("  After 5 ops: " + String(ESP.getFreeHeap()));
}
```

**Expected Output**:
```
[TEST] Memory:
  Free heap: 298432
  After 5 ops: 297856
```

**Pass Criteria**: 
- ✅ Heap usage < 5 KB per operation
- ✅ No memory leaks (heap stabilizes)

---

## Phase 6: Error Handling Tests

### Test 6.1: Missing PSK

**Objective**: Verify graceful handling of missing PSK

**Steps**:
1. Set PSK to empty string in config.json
2. Upload firmware
3. Check serial output

**Expected Output**:
```
[Security] ERROR: PSK is empty or invalid
[Security] Security layer initialization FAILED
[Device] Running in UNSECURED mode
```

**Pass Criteria**: 
- ✅ Error logged clearly
- ✅ Device continues running (fallback)

---

### Test 6.2: LittleFS Failure

**Objective**: Verify handling when filesystem fails

**Steps**:
1. Comment out LittleFS.begin() in security_layer.cpp
2. Upload firmware
3. Check serial output

**Expected Output**:
```
[Security] WARNING: Could not load nonce state
[Security] Starting with nonce = 1
[Security] Security layer initialized (nonce reset)
```

**Pass Criteria**: 
- ✅ Warning logged
- ✅ Nonce starts from 1
- ✅ Security still functional

---

### Test 6.3: Server Unreachable

**Objective**: Verify handling when server is down

**Steps**:
1. Stop cloud_server.py
2. Wait for upload attempt
3. Check serial output

**Expected Output**:
```
[Uplink] Securing upload payload...
[Security] Message secured
[HTTP] POST /api/inverter/readings
[HTTP] ERROR: Connection refused
[Uplink] Upload failed, will retry
```

**Pass Criteria**: 
- ✅ Security layer works even if upload fails
- ✅ Device retries later
- ✅ No crashes

---

## Test Results Checklist

### Basic Functionality
- [ ] Security layer initializes ✅
- [ ] HMAC computation works ✅
- [ ] Nonces are sequential ✅
- [ ] Nonces persist across reboots ✅

### Message Security
- [ ] Can create secured messages ✅
- [ ] Round-trip verification works ✅

### Attack Prevention
- [ ] Replay attacks detected ✅
- [ ] MAC tampering detected ✅
- [ ] Old nonces rejected ✅

### Integration
- [ ] Data upload secured ✅
- [ ] Config requests secured ✅
- [ ] Commands secured ✅

### Performance
- [ ] Overhead acceptable ✅
- [ ] Memory usage acceptable ✅

### Error Handling
- [ ] Missing PSK handled ✅
- [ ] Filesystem errors handled ✅
- [ ] Network errors handled ✅

---

## Troubleshooting

### Issue: "HMAC verification failed"

**Possible Causes**:
- PSK mismatch between device and server
- Nonce desynchronization
- Payload corruption

**Solutions**:
1. Verify PSK matches exactly on both sides
2. Reset nonce state (delete `/security/nonce.dat`)
3. Check Base64 encoding/decoding

---

### Issue: "Replay detected" for valid messages

**Possible Causes**:
- Nonce window too small
- Device rebooted and server has newer nonce

**Solutions**:
1. Increase `nonce_window` in config
2. Synchronize nonces between device and server
3. Disable `strict_nonce_checking` temporarily

---

### Issue: High memory usage

**Possible Causes**:
- JSON documents too large
- Memory leaks in security layer

**Solutions**:
1. Reduce payload sizes
2. Increase heap monitoring
3. Check for String memory leaks

---

## Final Validation

Before submission, ensure **ALL** tests pass:

```
✅ Phase 1: Basic Functionality (3/3)
✅ Phase 2: Message Security (2/2)
✅ Phase 3: Attack Tests (3/3)
✅ Phase 4: Integration (3/3)
✅ Phase 5: Performance (2/2)
✅ Phase 6: Error Handling (3/3)

Total: 16/16 tests passed
```

---

## Additional Resources

- **Architecture**: See `MILESTONE4_PART3_DOCUMENTATION.md`
- **Integration**: See `SECURITY_INTEGRATION_GUIDE.md`
- **Diagrams**: See `SECURITY_DIAGRAMS.md`
- **Implementation**: See `security_layer.hpp/cpp`

---

## Support

If tests fail:
1. Check serial output for error messages
2. Verify configuration matches guide
3. Review integration steps
4. Check server logs for details
5. Test with security disabled to isolate issues

**Good luck with testing!** 🚀
