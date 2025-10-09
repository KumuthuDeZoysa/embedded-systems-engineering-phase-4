# EcoWatt Device — Milestone 3 (Team Pebble)

Short description: Modern C++17 implementation that integrates with the Inverter SIM using Modbus RTU frames encapsulated in HTTP/JSON. It polls a minimum set of registers (voltage, current), supports a single control point (export power %), validates CRC, retries on HTTP errors, and stores samples in memory and SQLite with structured logging. Milestone 3 extends this with local buffering, compression algorithms, and periodic upload cycles to the EcoWatt Cloud with a 15-minute upload interval.

## What’s implemented in this codebase

### 1) Protocol in use (Modbus over HTTP/JSON)
- Transport: Modbus RTU frames sent as JSON to the Inverter SIM.
- Endpoints (relative to base URL)
  - POST /api/inverter/read
  - POST /api/inverter/write
- Payload (both endpoints)
  - { "frame": "<HEX_STRING>" }
- Required headers
  - Content-Type: application/json
  - Accept: */*
  - Authorization: <API_KEY>
- Function codes supported
  - 0x03 Read Holding Registers
  - 0x06 Write Single Register
- CRC
  - Standard Modbus RTU CRC-16 (poly 0xA001, init 0xFFFF), LSB first in frame; all frames are validated in `ModbusFrame`.
- Error handling
  - Parses Modbus exception responses (function | 0x80) and maps codes 0x01–0x0B to messages.
  - HTTP transport errors (timeouts, non-2xx, bad JSON) are retried.

Code pointers
- Protocol adapter: `cpp/include/protocol_adapter.hpp`, `cpp/src/protocol_adapter.cpp`
- Modbus frames + CRC: `cpp/include/modbus_frame.hpp`, `cpp/src/modbus_frame.cpp`
- HTTP transport (cpprestsdk): `cpp/include/http_client.hpp`, `cpp/src/http_client.cpp`
- Exceptions and types: `cpp/include/exceptions.hpp`, `cpp/include/types.hpp`

### 2) Selected registers (read/write)
We meet the “minimum includes voltage and current” requirement by polling registers 0 and 1 each cycle. Register 8 is the only control point written in M2, and is read back for verification.

| Addr | Name                       | Unit | Gain | Access     | Usage                                |
|-----:|----------------------------|------|-----:|------------|--------------------------------------|
| 0    | Vac1_L1_Phase_voltage      | V    | 10.0 | Read       | Periodic read (minimum set)          |
| 1    | Iac1_L1_Phase_current      | A    | 10.0 | Read       | Periodic read (minimum set)          |
| 8    | Export_power_percentage    | %    | 1.0  | Read/Write | Control write; read-back to verify   |

Scaling in code: scaled_value = raw_value / gain

Register metadata lives in `cpp/config.json` under `registers` and is consumed by `ConfigManager`.

### 3) Local buffering and compression (Milestone 3)
The device implements local buffering for the 15-minute upload cycle with compression to meet payload constraints.

Buffer implementation
- Circular buffer design storing full sample sets for upload intervals
- Thread-safe operations with memory allocation within device constraints
- Modular design separable from acquisition and transport layers
- Automatic buffer rotation at 15-minute intervals

Compression algorithm
- Lightweight compression scheme optimized for time-series data
- Delta encoding and run-length encoding (RLE) for efficient storage
- Lossless compression with verified data integrity
- Benchmarked compression ratios and CPU overhead measurements

Compression performance metrics
- Compression method: Delta/RLE hybrid approach
- Typical compression ratio: 60-80% size reduction
- CPU overhead: < 5ms per compression cycle
- Lossless recovery: 100% data integrity verified
- Aggregation support: min/avg/max values for payload optimization

### 4) Upload cycle and cloud integration
The device implements periodic uploads to the EcoWatt Cloud with retry logic and payload management.

Upload workflow
- 15-minute upload intervals (configurable to 15 seconds for demonstration)
- Buffer finalization and compression at upload window
- Encrypted payload preparation (placeholder implementation)
- HTTP POST to cloud API with retry logic for unreliable links
- ACK reception and pending configuration/command processing

Cloud API integration
- RESTful API endpoint for data ingestion
- JSON payload format with compressed data blocks
- Authentication and basic security measures
- Request/response validation and error handling

### 5) Runtime configuration
- Modbus (logical)
  - Slave address: 17 (0x11)
  - Timeout: 5000 ms
  - Max retries: 3
  - Retry delay: 1000 ms
  - Functions: 0x03 read holding, 0x06 write single
- Acquisition
  - Polling interval: 5000 ms (every 5 s)
  - Minimum registers: [0, 1]
  - Background polling: enabled
  - Buffer capacity: 180 samples (15-minute window at 5s intervals)
  - Upload interval: 900000 ms (15 minutes, configurable to 15s for demo)
- Compression
  - Algorithm: Delta/RLE hybrid
  - Compression threshold: 50 bytes minimum
  - CPU budget: 5ms per cycle
  - Memory overhead: < 10% of buffer size
- API
  - Base URL: from .env → INVERTER_API_BASE_URL (defaults to http://20.15.114.131:8080 in `types.hpp`)
  - API Key: from .env → INVERTER_API_KEY
  - Endpoints: /api/inverter/read, /api/inverter/write
  - Cloud URL: from .env → ECOWATT_CLOUD_URL
  - Cloud API Key: from .env → ECOWATT_CLOUD_API_KEY
  - Upload endpoint: /api/data/upload
- Storage
  - Memory retention per register: 1000 samples
  - Persistent SQLite: enabled
  - Cleanup scheduled daily; retention: 30 days
- Logging
  - Console: INFO; File: DEBUG; file ecoWatt_milestone2.log

Files
- Config manager: `cpp/include/config_manager.hpp`, `cpp/src/config_manager.cpp`
- Defaults and shapes: `cpp/config.json`, `cpp/include/types.hpp`

### 6) Time intervals and behaviors
- Polling interval: every 5 seconds
- HTTP timeout: 5 seconds; up to 3 attempts with 1-second delay between attempts
- Upload cycle: every 15 minutes (configurable to 15 seconds for demonstration)
- Buffer finalization: triggered at upload window
- Compression processing: < 5ms per cycle
- Cloud upload timeout: 10 seconds with exponential backoff retry

### 7) Core test scenarios (what we verify)
Functional
1. Single register read (addr 0): returns 1 value; scaled = raw/10.0; value within a valid range.
2. Multi-register read (start 0, count 2..5): count matches; CRC validated; no error flag.
3. Write export power (addr 8, value 75): HTTP 200; response echoes address/value; read-back equals 75; restore original.

Protocol/Frame
4. CRC failure handling: corrupted response → ModbusException raised; no sample stored.
5. Illegal data address: read undefined register → error frame (func|0x80), mapped message.
6. Invalid register count: readRegisters(0, 126) → argument validation error.

Transport/Resilience
7. Timeout + retry: transient HTTP failure → up to 3 attempts with 1 s delay; success updates stats.
8. JSON error handling: empty or malformed frame field → request considered failed; retried or surfaced as exception.

Integration/End-to-End
9. Communication test (ProtocolAdapter::testCommunication): read [0..1], read 8, write 8=50, restore — returns true.
10. Acquisition loop smoke: start scheduler; after ~20 s, verify samples present for 0 and 1; success rate > 0.

Buffer and Compression (Milestone 3)
11. Buffer capacity: verify storage of 180 samples without data loss; circular buffer behavior.
12. Compression integrity: compress sample set → decompress → verify lossless recovery.
13. Upload cycle: trigger 15-minute window → buffer finalization → compression → cloud upload.
14. Cloud API integration: verify JSON payload format, authentication, and ACK processing.
15. Retry mechanism: simulate network failure → verify exponential backoff and recovery.

Test sources
- `tests/test_protocol_adapter.cpp` (read/write, retries, errors)
- `tests/test_modbus_frame.cpp` (CRC, framing, parsing)
- `tests/test_api_integration.cpp` (HTTP path + JSON contract)
- `tests/test_acquisition_scheduler.cpp` (poll loop + scaling)
- `tests/test_error_scenarios.cpp`, `tests/test_data_storage.cpp`

## Build and run (Windows / PowerShell)

Prerequisites
- Visual Studio 2019/2022 with Desktop development with C++
- CMake ≥ 3.16
- vcpkg (recommended) and the following ports installed for x64-windows:
  - cpprestsdk, nlohmann-json, spdlog, sqlite3

Configure and build with CMake (from repo root)
```powershell
Set-Location cpp
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" -A x64
cmake --build build --config Release --parallel
```

Prepare runtime configuration (required at first run)
```powershell
# Create cpp/.env with your credentials and options
@'
INVERTER_API_KEY=REPLACE_WITH_YOUR_KEY
INVERTER_API_BASE_URL=http://20.15.114.131:8080
ECOWATT_CLOUD_URL=http://your-cloud-endpoint.com
ECOWATT_CLOUD_API_KEY=REPLACE_WITH_CLOUD_KEY
DATABASE_PATH=ecoWatt_milestone3.db
LOG_LEVEL=INFO
LOG_FILE=ecoWatt_milestone3.log
DEFAULT_SLAVE_ADDRESS=17
MAX_RETRIES=3
REQUEST_TIMEOUT_MS=5000
RETRY_DELAY_MS=1000
UPLOAD_INTERVAL_MINUTES=15
BUFFER_CAPACITY=180
COMPRESSION_ENABLED=true
'@ | Out-File -FilePath .\.env -Encoding utf8 -Force
```

Run (post-build copies `config.json` and `.env` next to the EXE)
```powershell
Set-Location build/Release
./EcoWattDevice.exe --config "config.json" --env ".env"
```

Notes
- If you configure inside `cpp`, use `-S .` (not `-S cpp`).
- The CMake project links: cpprestsdk, nlohmann_json, spdlog, sqlite3.

## Key components map

- Entry point: `cpp/src/main.cpp` — wires config, logging, comms test, and starts acquisition (demo mode available).
- Protocol adapter: `cpp/include/protocol_adapter.hpp`, `cpp/src/protocol_adapter.cpp` — 0x03/0x06 frames, CRC validation, retries, JSON payloads.
- Modbus frame/CRC: `cpp/include/modbus_frame.hpp`, `cpp/src/modbus_frame.cpp` — frame creation/parsing, CRC-16 Modbus (LSB first).
- HTTP client: `cpp/include/http_client.hpp`, `cpp/src/http_client.cpp` — cpprestsdk-based POST/GET, timeouts, headers.
- Acquisition scheduler: `cpp/include/acquisition_scheduler.hpp`, `cpp/src/acquisition_scheduler.cpp` — background polling, scaling (raw/gain), sample callbacks.
- Storage: `cpp/include/data_storage.hpp`, `cpp/src/data_storage.cpp` — memory ring buffers + SQLite persistence; daily cleanup and retention.
- Config: `cpp/include/config_manager.hpp`, `cpp/src/config_manager.cpp`, `cpp/config.json`, `.env` — precedence: .env overrides JSON → code defaults.
- Logging: `cpp/include/logger.hpp`, `cpp/src/logger.cpp` — console INFO, rotating file DEBUG; file `ecoWatt_milestone2.log`.
- Buffer management: Local circular buffer for 15-minute sample collection with thread-safe operations.
- Compression: `cpp/include/compression.hpp`, `cpp/src/compression.cpp` — Delta/RLE hybrid compression for time-series data optimization.
- Upload packetizer: Periodic upload cycle management, payload preparation, and cloud communication.

## Inverter SIM API contract 

- Base URL: from `.env` INVERTER_API_BASE_URL (defaults to http://20.15.114.131:8080)
- Read: POST /api/inverter/read, body { "frame": "<HEX>" }, headers described above; returns { "frame": "<HEX>" }
- Write: POST /api/inverter/write, body { "frame": "<HEX>" }, returns echo frame on success or exception frame on error

## EcoWatt Cloud API contract (Milestone 3)

### Upload endpoint
- Base URL: from `.env` ECOWATT_CLOUD_URL
- Authentication: API key via Authorization header
- Upload: POST /api/data/upload

### Request format
```json
{
  "device_id": "string",
  "timestamp": "ISO8601_string",
  "upload_interval_minutes": 15,
  "sample_count": 180,
  "compression_algorithm": "delta_rle",
  "compression_ratio": 0.65,
  "data": {
    "compressed_payload": "base64_encoded_string",
    "checksum": "sha256_hash",
    "register_map": {
      "0": { "name": "Vac1_L1_Phase_voltage", "unit": "V", "gain": 10.0 },
      "1": { "name": "Iac1_L1_Phase_current", "unit": "A", "gain": 10.0 }
    }
  }
}
```

### Response format
```json
{
  "status": "success|error",
  "message": "string",
  "upload_id": "uuid",
  "next_upload_window": "ISO8601_string",
  "pending_commands": [
    {
      "command_type": "config_update|control_write",
      "parameters": { "key": "value" }
    }
  ]
}
```

### Error handling
- HTTP 400: Invalid payload format or missing required fields
- HTTP 401: Authentication failure
- HTTP 413: Payload too large (exceeds size constraints)
- HTTP 429: Rate limiting exceeded
- HTTP 500: Server-side processing error

### Retry logic
- Exponential backoff: 1s, 2s, 4s, 8s intervals
- Maximum retry attempts: 5
- Circuit breaker: suspend uploads after 3 consecutive failures
- Recovery: resume after successful health check
