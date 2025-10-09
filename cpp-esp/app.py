# EcoWatt Cloud API (Flask Example) — 15s inactivity debounce (per device)

from flask import Flask, request, jsonify
import struct
import datetime
import threading
import time

app = Flask(__name__)

# In-memory stores
UPLOADS = []
BENCHMARKS = []

# Simple register map for simulator
SIM_REGISTERS = {}
SIM_EXCEPTIONS = set()

# -------- Debounced batching state (per device) --------
FLUSH_INTERVAL_SEC = 15

# BUFFERS[device_id] = {
#   "reg_values": { reg_addr: [values...] },
#   "received_bytes": int,
#   "last_seen": float (epoch seconds)  # when we last received data for this device
# }
BUFFERS = {}
BUFF_LOCK = threading.Lock()

def _now_epoch() -> float:
    return time.time()

def _flush_device_unlocked(device_id: str):
    """
    Compute per-register averages for a device buffer and push one record.
    CALL ONLY WITH BUFF_LOCK HELD.
    """
    buf = BUFFERS.get(device_id)
    if not buf:
        return None

    reg_values = buf.get("reg_values", {})
    received_bytes = int(buf.get("received_bytes", 0))

    # Nothing to flush?
    has_any = any(len(vals) for vals in reg_values.values())
    if not has_any:
        # keep buffer but reset counters
        BUFFERS[device_id] = {
            "reg_values": {},
            "received_bytes": 0,
            "last_seen": buf.get("last_seen", _now_epoch()),
        }
        return None

    # Build averaged samples (one per register with values)
    flush_dt = datetime.datetime.now().isoformat()
    averaged_samples = []
    values_flat = []
    for reg, vals in sorted(reg_values.items()):
        if not vals:
            continue
        avg_val = sum(vals) / len(vals)
        values_flat.extend(vals)
        averaged_samples.append({
            "timestamp": int(_now_epoch()),    # flush time as sample ts
            "reg_addr": reg,
            "value": round(float(avg_val), 3)
        })

    # Save upload record (this is what Node-RED reads)
    upload_record = {
        "timestamp": flush_dt,
        "device_id": device_id,
        "bytes": received_bytes,
        "samples": averaged_samples
    }
    UPLOADS.append(upload_record)

    # Prepare benchmark for Node-RED push
    num_samples = len(averaged_samples)
    original_size = num_samples * 4 * 3  # keep your earlier convention
    compressed_size = received_bytes
    compression_ratio = (round(original_size / compressed_size, 2)
                         if compressed_size > 0 else "N/A")
    min_v = min(values_flat) if values_flat else None
    max_v = max(values_flat) if values_flat else None
    avg_v = (round(sum(values_flat) / len(values_flat), 2)
             if values_flat else None)

    benchmark = {
        "method": "delta-avg-15s-inactivity",
        "num_samples": num_samples,
        "original_size": original_size,
        "compressed_size": compressed_size,
        "compression_ratio": compression_ratio,
        "lossless_verified": False,  # aggregate, not raw
        "cpu_time_ms": None,
        "min": min_v,
        "avg": avg_v,
        "max": max_v
    }

    flask_push_payload = {
        "device_id": device_id,
        "timestamp": flush_dt,
        "benchmark": benchmark,
        "samples": [s["value"] for s in averaged_samples]
    }

    # Reset buffer AFTER we've computed everything
    BUFFERS[device_id] = {
        "reg_values": {},
        "received_bytes": 0,
        "last_seen": _now_epoch(),  # sets a new baseline of inactivity
    }

    # Push to Node-RED (same endpoint/body as before)
    try:
        import requests
        node_red_url = "http://localhost:1880/api/flask_push"
        resp = requests.post(node_red_url, json=flask_push_payload, timeout=2)
        print(f"[DEBUG] flush->Node-RED: device={device_id}, status={resp.status_code}")
    except Exception as e:
        print(f"[ERROR] flush->Node-RED failed: {e}")

    return upload_record

def _flusher_loop():
    """Flush a device only after 15s of INACTIVITY since its last upload."""
    print(f"[INFO] Flusher thread started (debounce interval={FLUSH_INTERVAL_SEC}s)")
    while True:
        time.sleep(1)
        try:
            now = _now_epoch()
            with BUFF_LOCK:
                # copy keys to avoid mutation during iteration
                items = list(BUFFERS.items())
            for device_id, buf in items:
                last_seen = buf.get("last_seen", 0.0)
                has_data = any(len(v) for v in buf.get("reg_values", {}).values())
                if has_data and (now - last_seen) >= FLUSH_INTERVAL_SEC:
                    with BUFF_LOCK:
                        _flush_device_unlocked(device_id)
        except Exception as e:
            print(f"[ERROR] flusher loop: {e}")

# ---------------- Existing endpoints (unchanged signatures) ----------------

@app.route('/api/upload/meta', methods=['POST'])
def upload_meta():
    try:
        meta = request.get_json(force=True)
        print(f"[BENCHMARK] Compression Method: {meta.get('compression_method')}")
        print(f"[BENCHMARK] Number of Samples: {meta.get('num_samples')}")
        print(f"[BENCHMARK] Original Payload Size: {meta.get('original_size')}")
        print(f"[BENCHMARK] Compressed Payload Size: {meta.get('compressed_size')}")
        print(f"[BENCHMARK] Compression Ratio: {meta.get('compression_ratio')}")
        print(f"[BENCHMARK] CPU Time (ms): {meta.get('cpu_time_ms')}")
        print(f"[BENCHMARK] Lossless Recovery Verification: {meta.get('lossless')}")
        print(f"[BENCHMARK] Aggregation min/avg/max: {meta.get('min')}, {meta.get('avg')}, {meta.get('max')}")
        BENCHMARKS.append(meta)
        return jsonify({'status': 'success', 'benchmark': meta})
    except Exception as e:
        print(f"[ERROR] /api/upload/meta: {e}")
        return jsonify({'status': 'error', 'error': str(e)}), 400

def decompress_delta_payload(data: bytes):
    """
    Decompresses the binary payload from the device.
    Format for each sample:
      [timestamp (4 bytes, uint32 LE)]
      [reg_addr (1 byte)]
      [value (4 bytes, float32 LE)]
    """
    print(f"[DEBUG] decompress_delta_payload: received {len(data)} bytes")
    sample_size = 4 + 1 + 4
    if len(data) < sample_size:
        print("[DEBUG] decompress_delta_payload: data too short")
        return []

    samples = []
    offset = 0
    idx = 0
    while offset + sample_size <= len(data):
        ts, reg_addr, value = struct.unpack('<IBf', data[offset:offset+sample_size])
        print(f"[DEBUG] decompress_delta_payload: sample {idx}: ts={ts}, reg_addr={reg_addr}, value={value}")
        samples.append({'timestamp': int(ts), 'reg_addr': reg_addr, 'value': round(float(value), 3)})
        offset += sample_size
        idx += 1
    print(f"[DEBUG] decompress_delta_payload: total samples={len(samples)}")
    return samples

@app.route('/api/upload', methods=['POST'])
def upload():
    # Authentication removed for testing
    compressed_payload = request.data
    device_id = request.headers.get('device-id') or request.headers.get('Device-ID') or 'Unknown-Device'
    print(f"[DEBUG] /api/upload called: device_id={device_id}, payload_bytes={len(compressed_payload)}")

    try:
        samples = decompress_delta_payload(compressed_payload)
        print(f"[DEBUG] /api/upload: decompressed {len(samples)} samples")
    except Exception as e:
        print(f"[ERROR] /api/upload: decompression failed: {e}")
        samples = []

    # Accumulate into debounce buffer (NO immediate push)
    now = _now_epoch()
    with BUFF_LOCK:
        buf = BUFFERS.get(device_id)
        if not buf:
            buf = {"reg_values": {}, "received_bytes": 0, "last_seen": now}
            BUFFERS[device_id] = buf

        for s in samples:
            reg = s.get("reg_addr")
            val = s.get("value")
            if reg is None or val is None:
                continue
            buf["reg_values"].setdefault(reg, []).append(float(val))

        buf["received_bytes"] += int(len(compressed_payload))
        buf["last_seen"] = now  # debounce: reset the inactivity timer on every upload

    # Immediate ACK; flush occurs after 15s of inactivity
    return jsonify({'status': 'success', 'received': len(compressed_payload)})

@app.route('/api/uploads', methods=['GET'])
def get_uploads():
    # Expose flushed records and benchmark meta
    return jsonify({'uploads': UPLOADS, 'benchmarks': BENCHMARKS})

# ----------------- Modbus simulator (unchanged) -----------------

def compute_crc(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF

@app.route('/api/inverter/read', methods=['POST'])
def inverter_read():
    # Expecting JSON: {"frame": "<HEX>"}
    j = request.get_json(silent=True)
    if not j or 'frame' not in j:
        return jsonify({'error': 'no frame provided'}), 400
    hex_frame = j['frame']
    try:
        data = bytes.fromhex(hex_frame)
    except Exception:
        return jsonify({'error': 'invalid hex'}), 400

    if len(data) < 8:
        return jsonify({'error': 'frame too short'}), 400
    slave = data[0]
    func = data[1]

    if func == 0x03:
        num_regs = (data[4] << 8) | data[5]
        start_addr = (data[2] << 8) | data[3]
        if start_addr in SIM_EXCEPTIONS:
            payload = bytearray()
            payload.append(slave)
            payload.append(0x83)
            payload.append(0x02)
            crc = compute_crc(payload)
            payload.append(crc & 0xFF)
            payload.append((crc >> 8) & 0xFF)
            return jsonify({'frame': payload.hex().upper()})
        byte_count = num_regs * 2
        payload = bytearray()
        payload.append(slave)
        payload.append(0x03)
        payload.append(byte_count)
        for i in range(num_regs):
            addr = start_addr + i
            if addr in SIM_REGISTERS:
                val = SIM_REGISTERS[addr]
            else:
                if addr == 0:
                    val = 2300
                elif addr == 1:
                    val = 25
                elif addr == 2:
                    val = 5000
                elif addr == 3:
                    val = 3200
                elif addr == 4:
                    val = 3150
                elif addr == 5:
                    val = 85
                elif addr == 6:
                    val = 82
                elif addr == 7:
                    val = 350
                elif addr == 8:
                    val = 75
                elif addr == 9:
                    val = 1850
                else:
                    val = (i + 1) * 10
            payload.append((val >> 8) & 0xFF)
            payload.append(val & 0xFF)
        crc = compute_crc(payload)
        payload.append(crc & 0xFF)
        payload.append((crc >> 8) & 0xFF)
        return jsonify({'frame': payload.hex().upper()})
    else:
        return jsonify({'error': 'unsupported function in sim'}), 400

@app.route('/api/inverter/write', methods=['POST'])
def inverter_write():
    j = request.get_json(silent=True)
    if not j or 'frame' not in j:
        return jsonify({'error': 'no frame provided'}), 400
    hex_frame = j['frame']
    try:
        data = bytes.fromhex(hex_frame)
    except Exception:
        return jsonify({'error': 'invalid hex'}), 400

    if len(data) >= 6:
        core = data[:-2]
        crc = compute_crc(core)
        resp = bytearray(core)
        resp.append(crc & 0xFF)
        resp.append((crc >> 8) & 0xFF)
        return jsonify({'frame': resp.hex().upper()})
    return jsonify({'error': 'frame too short'}), 400

@app.route('/api/inverter/config', methods=['POST'])
def inverter_config():
    j = request.get_json(silent=True)
    if not j:
        return jsonify({'error': 'invalid json'}), 400
    regs = j.get('registers')
    if regs:
        for k, v in regs.items():
            try:
                addr = int(k)
                SIM_REGISTERS[addr] = int(v)
            except Exception:
                pass
    ex = j.get('exceptions')
    if ex is not None:
        SIM_EXCEPTIONS.clear()
        for a in ex:
            try:
                SIM_EXCEPTIONS.add(int(a))
            except Exception:
                pass
    return jsonify({'status': 'ok', 'registers': SIM_REGISTERS, 'exceptions': list(SIM_EXCEPTIONS)})

# ---------------- Startup ----------------

def _start_flusher_once():
    t = threading.Thread(target=_flusher_loop, daemon=True)
    t.start()

_start_flusher_once()

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080)
