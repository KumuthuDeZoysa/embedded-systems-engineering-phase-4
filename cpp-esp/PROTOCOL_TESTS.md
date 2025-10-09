# Protocol Documentation & Test Scenarios

## Protocol Adapter
- **Read Registers**: Sends Modbus frame via HTTP POST to `/read_endpoint`. Expects comma-separated register values in response.
- **Write Register**: Sends Modbus frame via HTTP POST to `/write_endpoint`. Expects success/failure response.
- **Error Handling**: Retries up to 3 times on failure.

## Data Acquisition
- **Polling**: Configurable interval (default 5s). Polls voltage/current registers.
- **Buffering**: Samples stored in circular buffer. Aggregation (min/avg/max) supported.

## Compression & Upload
- **Compression**: Delta encoding of sample values before upload.
- **Upload**: Every 15 minutes (demo: 15s), chunked upload with retry to `/api/upload`.

## Test Scenarios
1. **Read Test**: Simulate Inverter SIM response, verify correct parsing and storage.
2. **Write Test**: Send write command, verify response and retry on failure.
3. **Buffer Test**: Fill buffer, check aggregation and compression output.
4. **Upload Test**: Send compressed data to Flask server, verify receipt and chunking.
5. **Error Recovery**: Simulate HTTP failure, verify retry logic and error logging.

---

Add results and screenshots for each test to this document for submission.