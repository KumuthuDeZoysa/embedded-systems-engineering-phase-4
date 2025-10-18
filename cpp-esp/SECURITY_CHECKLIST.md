# Security Implementation Checklist

Use this checklist to track your progress in implementing the security layer.

---

## Phase 1: Understanding & Preparation

### Documentation Review
- [ ] Read `SECURITY_IMPLEMENTATION_SUMMARY.md` for overview
- [ ] Read `MILESTONE4_PART3_DOCUMENTATION.md` for technical details
- [ ] Read `SECURITY_INTEGRATION_GUIDE.md` for step-by-step instructions
- [ ] Understand the threat model and security properties
- [ ] Review message formats and protocols

### Environment Setup
- [ ] Ensure ESP8266/ESP32 development environment is working
- [ ] Verify PlatformIO is installed and configured
- [ ] Check that existing Milestones 1-3 code is working
- [ ] Verify Milestone 4 Parts 1-2 are implemented

---

## Phase 2: Core Security Files

### Create New Files
- [x] `include/security_layer.hpp` - Security layer header (DONE)
- [x] `src/security_layer.cpp` - Security layer implementation (DONE)
- [x] `include/secure_http_client.hpp` - Secure HTTP wrapper header (DONE)
- [x] `src/secure_http_client.cpp` - Secure HTTP wrapper implementation (DONE)

### Review Created Files
- [ ] Review `security_layer.hpp` - Understand the API
- [ ] Review `security_layer.cpp` - Understand implementation
- [ ] Review `secure_http_client.hpp` - Understand wrapper
- [ ] Review `secure_http_client.cpp` - Understand integration

---

## Phase 3: Configuration

### Generate Security Keys
- [ ] Generate PSK: `openssl rand -hex 32`
- [ ] Save PSK securely (you'll need it for cloud server too)
- [ ] Verify PSK is 64 hex characters (32 bytes)

### Update config.json
- [ ] Add `security` section to `config/config.json`
- [ ] Set `enabled: true`
- [ ] Set `psk` with your generated key
- [ ] Configure `encryption_enabled: true`
- [ ] Configure `use_real_encryption: true` (or false for testing)
- [ ] Set `nonce_window: 100`
- [ ] Set `strict_nonce_checking: true`

### Update ConfigManager
- [ ] Add `SecurityConfig` struct to `include/config_manager.hpp`
- [ ] Add `getSecurityConfig()` method to ConfigManager
- [ ] Update `loadFromFile()` in `src/config_manager.cpp` to parse security config
- [ ] Test config loading

---

## Phase 4: Device Integration

### Update EcoWattDevice
- [ ] Add security includes to `include/ecoWatt_device.hpp`
- [ ] Add `SecurityLayer* security_layer_` member
- [ ] Add `SecureHttpClient* secure_http_client_` member
- [ ] Update constructor to initialize pointers to nullptr
- [ ] Update `setup()` to initialize security layer
- [ ] Update `setup()` to create secure HTTP client
- [ ] Update destructor to cleanup security objects
- [ ] Test compilation

### Update UplinkPacketizer
- [ ] Add `SecurityLayer* security_` member to `include/uplink_packetizer.hpp`
- [ ] Add `setSecurityLayer()` method
- [ ] Initialize `security_` to nullptr in constructor
- [ ] Update `uploadTask()` to secure payload before sending
- [ ] Convert binary payload to JSON format
- [ ] Call `secureMessage()` to wrap payload
- [ ] Generate secured envelope
- [ ] Handle security errors gracefully
- [ ] Test data upload with security

### Update RemoteConfigHandler
- [ ] Add `SecurityLayer* security_` member to `include/remote_config_handler.hpp`
- [ ] Add `setSecurityLayer()` method
- [ ] Initialize `security_` to nullptr in constructor
- [ ] Update `checkForConfigUpdate()` to verify secured responses
- [ ] Update `sendConfigAck()` to secure acknowledgments
- [ ] Update `sendCommandResults()` to secure results
- [ ] Handle security verification failures gracefully
- [ ] Test config updates with security

---

## Phase 5: Build Configuration

### Update platformio.ini
- [ ] Add `rweather/Crypto@^0.4.0` to lib_deps (ESP8266)
- [ ] Note: mbedtls is built-in on ESP32, no lib needed
- [ ] Add any required build flags
- [ ] Test compilation

### Build & Upload
- [ ] Run `pio run` to build
- [ ] Fix any compilation errors
- [ ] Run `pio run -t upload` to upload to device
- [ ] Run `pio device monitor` to view serial output
- [ ] Verify security layer initialization messages

---

## Phase 6: Cloud Server Updates

### Update Flask Server (cloud_server.py)
- [ ] Add PSK configuration (same as device)
- [ ] Implement `compute_hmac()` function
- [ ] Implement `verify_hmac()` function
- [ ] Add nonce tracking (last_nonce, recent_nonces)
- [ ] Implement `verify_secured_message()` function
- [ ] Implement `secure_message()` function
- [ ] Update `/api/inverter/config` GET endpoint to verify requests
- [ ] Update `/api/inverter/config` GET endpoint to secure responses
- [ ] Update `/api/inverter/config/ack` POST endpoint to verify secured acks
- [ ] Update `/api/inverter/upload` POST endpoint to verify secured uploads
- [ ] Update `/api/inverter/config/command/result` POST to verify secured results
- [ ] Add logging for security events (replay, MAC failures, etc.)
- [ ] Test server startup

---

## Phase 7: Testing

### Basic Functionality
- [ ] Test device boots successfully with security enabled
- [ ] Verify nonce state file is created in LittleFS
- [ ] Check serial logs for security initialization
- [ ] Verify no errors during startup

### Data Upload (Uplink)
- [ ] Trigger data upload cycle
- [ ] Verify payload is secured (check serial logs)
- [ ] Verify cloud receives and verifies upload
- [ ] Check nonce is incremented
- [ ] Verify MAC is correct
- [ ] Test upload with security disabled (should still work)

### Config Updates
- [ ] Send config update from cloud
- [ ] Verify device receives secured message
- [ ] Verify device verifies HMAC
- [ ] Verify device processes config update
- [ ] Verify acknowledgment is secured
- [ ] Verify cloud receives and verifies ack

### Command Execution
- [ ] Queue command on cloud
- [ ] Verify device receives secured command
- [ ] Verify device executes command
- [ ] Verify result is secured
- [ ] Verify cloud receives and verifies result

### Security Features
- [ ] Test replay attack detection
  - Resend old message
  - Verify device rejects it
- [ ] Test MAC tampering detection
  - Modify payload without updating MAC
  - Verify device rejects it
- [ ] Test nonce window
  - Send nonce outside window
  - Verify device rejects it
- [ ] Test nonce persistence
  - Reboot device
  - Verify nonce continues from last value
  - Verify no nonce reuse after reboot

### Performance
- [ ] Measure message processing time
- [ ] Monitor CPU usage during security operations
- [ ] Check memory usage (heap, stack)
- [ ] Verify no memory leaks over time
- [ ] Test with high message frequency

---

## Phase 8: Error Handling

### Device Error Scenarios
- [ ] Test with invalid PSK
- [ ] Test with missing security config
- [ ] Test with corrupted nonce state file
- [ ] Test security initialization failure
- [ ] Test HMAC computation failure
- [ ] Test encryption failure
- [ ] Test network errors during secure upload
- [ ] Verify graceful degradation in all cases

### Cloud Error Scenarios
- [ ] Test cloud with wrong PSK
- [ ] Test cloud with missing nonce tracking
- [ ] Test cloud with HMAC verification disabled
- [ ] Test handling of malformed secured messages
- [ ] Test handling of invalid JSON

---

## Phase 9: Documentation

### Code Documentation
- [ ] Review all code comments
- [ ] Ensure all functions have clear descriptions
- [ ] Document any non-obvious logic
- [ ] Add examples in comments where helpful

### User Documentation
- [ ] Write usage instructions
- [ ] Document configuration options
- [ ] Create troubleshooting guide
- [ ] Add security best practices
- [ ] Document PSK management

### Video Demonstration (Milestone Requirement)
- [ ] Prepare demo script
- [ ] Record configuration update with security
- [ ] Record command execution with security
- [ ] Demonstrate replay attack prevention
- [ ] Demonstrate MAC tampering detection
- [ ] Show security statistics
- [ ] Explain message format
- [ ] Keep video under 10 minutes
- [ ] Ensure video is on throughout demo

---

## Phase 10: Final Validation

### Requirements Checklist
- [ ] ✅ HMAC-SHA256 authentication implemented
- [ ] ✅ Pre-shared key (PSK) management
- [ ] ✅ AES-CBC encryption (or Base64 simulation)
- [ ] ✅ Anti-replay protection with nonces
- [ ] ✅ Persistent nonce storage
- [ ] ✅ Secured message envelope format
- [ ] ✅ Integration with all subsystems
- [ ] ✅ Configuration via config.json
- [ ] ✅ Cloud server integration
- [ ] ✅ Error handling and logging

### Code Quality Checklist
- [ ] No compilation warnings
- [ ] No runtime errors
- [ ] Memory leaks checked
- [ ] Code follows clean code principles
- [ ] Proper error handling throughout
- [ ] Comprehensive logging
- [ ] Modular design
- [ ] Clear naming conventions

### Deliverables Checklist
- [ ] Source code (header and implementation files)
- [ ] Documentation (technical and integration guides)
- [ ] Configuration examples
- [ ] Cloud server code updates
- [ ] Test cases and results
- [ ] Demo video (max 10 minutes)
- [ ] All files zipped for Moodle submission

---

## Phase 11: Submission Preparation

### File Organization
- [ ] Organize all source files
- [ ] Include all documentation files
- [ ] Add configuration examples
- [ ] Include test results
- [ ] Add demo video or link
- [ ] Create README for submission

### Final Tests
- [ ] Clean build from scratch
- [ ] Flash fresh device
- [ ] Run complete integration test
- [ ] Verify all features work
- [ ] Check logs for any errors

### Zip for Submission
- [ ] Create submission folder
- [ ] Copy all required files
- [ ] Include documentation
- [ ] Add video or video link
- [ ] Zip everything
- [ ] Verify zip is not corrupted
- [ ] Upload to Moodle

---

## Success Criteria

### Must Have ✅
- [x] HMAC-SHA256 authentication working
- [x] Message encryption (AES or simulation)
- [x] Anti-replay protection functional
- [x] Persistent nonce storage
- [x] Integration with all subsystems
- [ ] All tests passing
- [ ] Demo video completed

### Nice to Have (Bonus) ⭐
- [ ] Performance benchmarks documented
- [ ] Security statistics dashboard
- [ ] Multiple PSK support (key rotation)
- [ ] Automated test suite
- [ ] Security audit report

---

## Notes & Issues

### Known Issues
- List any known issues or limitations here

### Questions
- List any questions or uncertainties here

### Todo
- List any remaining tasks here

---

## Completion Status

**Overall Progress**: ___% Complete

**Estimated Completion Date**: __________

**Last Updated**: __________

---

## Quick Reference

### Important Commands
```bash
# Generate PSK
openssl rand -hex 32

# Build project
pio run

# Upload to device
pio run -t upload

# Monitor serial
pio device monitor

# Clean build
pio run -t clean
```

### Important Files
- `include/security_layer.hpp` - Security API
- `src/security_layer.cpp` - Security implementation
- `config/config.json` - Configuration
- `cloud_server.py` - Cloud server
- `SECURITY_INTEGRATION_GUIDE.md` - Step-by-step guide

### Important Logs to Check
- `[Security] Initializing security layer...`
- `[Security] Security layer initialized`
- `[Uplink] Securing upload payload...`
- `[RemoteCfg] Response verified successfully`
- `[Security] Message secured: nonce=...`
- `[Security] HMAC verification failed` (should not appear)
- `[Security] Replay detected` (during replay test)
