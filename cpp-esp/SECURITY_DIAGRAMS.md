# Security Architecture Diagrams

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│                         EcoWatt Device                                │
│                                                                       │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    Application Layer                         │   │
│  │                                                              │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │   │
│  │  │   Remote     │  │   Command    │  │   Uplink     │     │   │
│  │  │   Config     │  │   Executor   │  │  Packetizer  │     │   │
│  │  │   Handler    │  │              │  │              │     │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘     │   │
│  │         │                 │                  │             │   │
│  └─────────│─────────────────│──────────────────│─────────────┘   │
│            │                 │                  │                  │
│            └─────────┬───────┴──────────────────┘                  │
│                      ▼                                             │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │              SecureHttpClient                                │   │
│  │  • Wraps all HTTP requests/responses                        │   │
│  │  • Automatic encryption/decryption                          │   │
│  │  • Transparent to application                               │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                      │                                             │
│                      ▼                                             │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │              SecurityLayer                                   │   │
│  │                                                              │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │   │
│  │  │     HMAC     │  │  Encryption  │  │    Nonce     │     │   │
│  │  │   SHA-256    │  │   AES-CBC    │  │  Management  │     │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘     │   │
│  │                                                              │   │
│  │  ┌──────────────────────────────────────────────────────┐  │   │
│  │  │  Persistent Storage (LittleFS)                       │  │   │
│  │  │  /security/nonce.dat                                 │  │   │
│  │  └──────────────────────────────────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                      │                                             │
│                      ▼                                             │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │              EcoHttpClient                                   │   │
│  │  • WiFi connectivity                                        │   │
│  │  • HTTP transport                                           │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                      │                                             │
└──────────────────────┼─────────────────────────────────────────────┘
                       │
                       │ Internet
                       │
                       ▼
┌──────────────────────────────────────────────────────────────────────┐
│                      EcoWatt Cloud Server                             │
│                                                                       │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │              Flask Application                               │   │
│  │                                                              │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │   │
│  │  │   Config     │  │   Command    │  │    Data      │     │   │
│  │  │   Endpoint   │  │   Endpoint   │  │   Upload     │     │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘     │   │
│  │         │                 │                  │             │   │
│  └─────────│─────────────────│──────────────────│─────────────┘   │
│            └─────────┬───────┴──────────────────┘                  │
│                      ▼                                             │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │           Security Verification Layer                        │   │
│  │  • verify_secured_message()                                 │   │
│  │  • secure_message()                                         │   │
│  │  • HMAC verification                                        │   │
│  │  • Nonce tracking                                           │   │
│  │  • Replay detection                                         │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                       │
└──────────────────────────────────────────────────────────────────────┘
```

---

## Message Flow: Secured Upload

```
Device                        Cloud
  │                             │
  │ 1. Generate data payload    │
  │                             │
  │ 2. Get next nonce           │
  │    nonce = 1234             │
  │                             │
  │ 3. Encrypt payload          │
  │    AES-CBC or Base64        │
  │                             │
  │ 4. Compute HMAC             │
  │    input = nonce +          │
  │            timestamp +      │
  │            encrypted_flag + │
  │            payload          │
  │    mac = HMAC-SHA256(input) │
  │                             │
  │ 5. Generate envelope        │
  │    {                        │
  │      "nonce": 1234,         │
  │      "timestamp": ...,      │
  │      "encrypted": true,     │
  │      "payload": "...",      │
  │      "mac": "..."           │
  │    }                        │
  │                             │
  │ 6. HTTP POST                │
  │────────────────────────────>│
  │                             │
  │                             │ 7. Parse envelope
  │                             │
  │                             │ 8. Check nonce
  │                             │    - Not seen before?
  │                             │    - Within window?
  │                             │
  │                             │ 9. Verify HMAC
  │                             │    compute expected MAC
  │                             │    compare with received
  │                             │
  │                             │ 10. Decrypt payload
  │                             │     AES-CBC or Base64
  │                             │
  │                             │ 11. Update nonce
  │                             │     last_nonce = 1234
  │                             │     recent_nonces.add(1234)
  │                             │
  │                             │ 12. Process data
  │                             │
  │                             │ 13. Generate response
  │                             │
  │                             │ 14. Secure response
  │                             │     (same process)
  │                             │
  │ 15. HTTP 200 OK            │
  │<────────────────────────────│
  │    {secured response}       │
  │                             │
  │ 16. Verify response         │
  │     - Check nonce           │
  │     - Verify HMAC           │
  │     - Decrypt payload       │
  │                             │
  │ 17. Process response        │
  │                             │
```

---

## Security Layer Internal Flow

```
┌─────────────────────────────────────────────────────────────┐
│              secureMessage(plain_payload)                    │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Get next nonce       │
              │  nonce = current_++   │
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Get timestamp        │
              │  ts = millis()        │
              └───────────────────────┘
                          │
                          ▼
          ┌───────────────────────────────┐
          │  Encrypt payload?             │
          └───────────────────────────────┘
                    │           │
            Yes─────┤           ├─────No
                    │           │
                    ▼           ▼
        ┌──────────────┐   ┌──────────────┐
        │  AES-CBC     │   │  Base64      │
        │  encryption  │   │  encoding    │
        └──────────────┘   └──────────────┘
                    │           │
                    └─────┬─────┘
                          ▼
              ┌───────────────────────┐
              │  Build HMAC input     │
              │  = nonce + ts +       │
              │    encrypted + payload│
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Compute HMAC-SHA256  │
              │  mac = HMAC(input)    │
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Build JSON envelope  │
              │  {nonce, ts, enc,     │
              │   payload, mac}       │
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Return SecuredMessage│
              └───────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│             verifyMessage(secured_json)                      │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Parse JSON envelope  │
              │  Extract: nonce, ts,  │
              │  encrypted, payload,  │
              │  mac                  │
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Check nonce          │
              │  - Seen before?       │
              │  - In window?         │
              └───────────────────────┘
                          │
                   Valid  │  Invalid
                    ┌─────┴─────┐
                    │           │
                    ▼           ▼
           ┌──────────────┐  ┌──────────────┐
           │   Continue   │  │    REJECT    │
           │              │  │  REPLAY_     │
           │              │  │  DETECTED    │
           └──────────────┘  └──────────────┘
                    │
                    ▼
              ┌───────────────────────┐
              │  Build HMAC input     │
              │  (same as sender)     │
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Compute HMAC         │
              │  expected_mac =       │
              │    HMAC(input)        │
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Compare MACs         │
              │  (constant-time)      │
              └───────────────────────┘
                          │
                   Match  │  Mismatch
                    ┌─────┴─────┐
                    │           │
                    ▼           ▼
           ┌──────────────┐  ┌──────────────┐
           │   Continue   │  │    REJECT    │
           │              │  │  INVALID_MAC │
           └──────────────┘  └──────────────┘
                    │
                    ▼
          ┌───────────────────────────────┐
          │  Decrypt payload?             │
          └───────────────────────────────┘
                    │           │
            Yes─────┤           ├─────No
                    │           │
                    ▼           ▼
        ┌──────────────┐   ┌──────────────┐
        │  AES-CBC     │   │  Base64      │
        │  decryption  │   │  decoding    │
        └──────────────┘   └──────────────┘
                    │           │
                    └─────┬─────┘
                          ▼
              ┌───────────────────────┐
              │  Update nonce         │
              │  last_nonce = nonce   │
              │  recent.add(nonce)    │
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Return plain payload │
              └───────────────────────┘
```

---

## Nonce Management

```
┌────────────────────────────────────────────────────────┐
│                 Nonce State                             │
│                                                         │
│  current_nonce_        = 1000  (next outgoing)        │
│  last_received_nonce_  = 0     (last accepted)        │
│  recent_nonces_        = []    (history, max 100)     │
│                                                         │
└────────────────────────────────────────────────────────┘
                          │
                          │ Device sends message
                          ▼
              ┌───────────────────────┐
              │  getNextNonce()       │
              │  returns 1000         │
              │  current_nonce_ = 1001│
              └───────────────────────┘
                          │
                          │ Every 10 nonces
                          ▼
              ┌───────────────────────┐
              │  saveNonceState()     │
              │  to LittleFS          │
              └───────────────────────┘

                          │
                          │ Device receives message
                          │ with nonce = 500
                          ▼
              ┌───────────────────────┐
              │  isNonceValid(500)?   │
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Check in recent[]?   │
              │  500 not in recent    │
              └───────────────────────┘
                          │
                          ▼
          ┌───────────────────────────────┐
          │  Strict checking?             │
          └───────────────────────────────┘
                    │           │
            Yes─────┤           ├─────No
                    │           │
                    ▼           ▼
        ┌──────────────┐   ┌──────────────┐
        │  500 <= 0?   │   │  Accept      │
        │  Yes - REJECT│   │              │
        └──────────────┘   └──────────────┘
                                 │
                                 ▼
                    ┌───────────────────────┐
                    │  updateLastNonce(500) │
                    │  last_received_ = 500 │
                    │  recent.add(500)      │
                    └───────────────────────┘
                                 │
                                 ▼
                    ┌───────────────────────┐
                    │  Maintain size        │
                    │  if len > 100:        │
                    │    recent.remove(0)   │
                    └───────────────────────┘

┌────────────────────────────────────────────────────────┐
│              Persistent Storage Format                  │
│                                                         │
│  Offset │ Size │ Field                                 │
│  ─────────────────────────────────────────────────────│
│    0    │  4   │ version (uint32_t)                   │
│    4    │  4   │ current_nonce (uint32_t)             │
│    8    │  4   │ last_received_nonce (uint32_t)       │
│   12    │  4   │ history_count (uint32_t)             │
│   16    │  4N  │ nonce_history[] (uint32_t × N)       │
│                                                         │
│  File: /security/nonce.dat                             │
│  Size: ~450 bytes (100 nonce history)                  │
└────────────────────────────────────────────────────────┘
```

---

## HMAC Computation Detail

```
┌────────────────────────────────────────────────────────┐
│              computeHMAC(data, key)                     │
└────────────────────────────────────────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Convert hex key      │
              │  to bytes             │
              │  "0123...def" ->      │
              │  {0x01, 0x23, ...}    │
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Initialize HMAC ctx  │
              │  (SHA-256)            │
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  HMAC_Start(key)      │
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  HMAC_Update(data)    │
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  HMAC_Finish()        │
              │  -> 32 bytes          │
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Convert to hex       │
              │  {0xAB, 0xCD, ...} -> │
              │  "abcd...ef"          │
              │  (64 hex chars)       │
              └───────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │  Return hex string    │
              └───────────────────────┘
```

---

## Integration Points

```
┌─────────────────────────────────────────────────────────┐
│                  Application Flow                        │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  RemoteConfigHandler::checkForConfigUpdate()            │
│  ┌───────────────────────────────────────────────────┐ │
│  │  Before: http_->get(endpoint)                     │ │
│  │  After:  secure_http_->secureGet(endpoint, plain) │ │
│  │                                                    │ │
│  │  Response automatically verified                  │ │
│  └───────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  RemoteConfigHandler::sendConfigAck()                   │
│  ┌───────────────────────────────────────────────────┐ │
│  │  Before: http_->post(endpoint, json, len)         │ │
│  │  After:  security_->secureMessage(json, secured)  │ │
│  │          envelope = generateEnvelope(secured)     │ │
│  │          http_->post(endpoint, envelope, len)     │ │
│  │                                                    │ │
│  │  Payload automatically encrypted & signed         │ │
│  └───────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  UplinkPacketizer::uploadTask()                         │
│  ┌───────────────────────────────────────────────────┐ │
│  │  Before: http_->post(url, compressed, len)        │ │
│  │  After:  json = wrapInJson(compressed)            │ │
│  │          security_->secureMessage(json, secured)  │ │
│  │          envelope = generateEnvelope(secured)     │ │
│  │          http_->post(url, envelope, len)          │ │
│  │                                                    │ │
│  │  Upload automatically encrypted & signed          │ │
│  └───────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

---

## Threat Mitigation

```
┌──────────────────────────────────────────────────────────┐
│                   Threat: Eavesdropping                   │
├──────────────────────────────────────────────────────────┤
│  Attacker:  Intercepts network traffic                   │
│  Mitigation: AES-CBC encryption                          │
│                                                           │
│  Plain -> Encrypt -> Base64 -> Transmit                  │
│                                                           │
│  Result: Attacker sees encrypted blob, cannot read       │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│               Threat: Man-in-the-Middle                   │
├──────────────────────────────────────────────────────────┤
│  Attacker:  Modifies message in transit                  │
│  Mitigation: HMAC-SHA256 integrity check                 │
│                                                           │
│  Message + MAC -> Transmit -> Receive -> Verify MAC      │
│                                                           │
│  Result: Modified message has invalid MAC, rejected      │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│                  Threat: Replay Attack                    │
├──────────────────────────────────────────────────────────┤
│  Attacker:  Replays valid old message                    │
│  Mitigation: Nonce tracking                              │
│                                                           │
│  Message(nonce=100) -> Store nonce -> Reject duplicates  │
│                                                           │
│  Result: Duplicate nonce detected, message rejected      │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│               Threat: Command Injection                   │
├──────────────────────────────────────────────────────────┤
│  Attacker:  Sends malicious command                      │
│  Mitigation: PSK authentication                          │
│                                                           │
│  Command -> Sign with PSK -> Verify -> Execute           │
│                                                           │
│  Result: Without PSK, attacker cannot create valid MAC   │
└──────────────────────────────────────────────────────────┘
```

---

This visual guide shows the complete security architecture and flows!
