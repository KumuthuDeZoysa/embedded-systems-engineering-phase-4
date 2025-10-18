import requests
import json
import hashlib
import base64

def upload_firmware():
    # Create simple test firmware
    firmware_data = b"EcoWatt-1.0.4-firmware-data" + b"\x00" * (35840 - 28)  # 35KB
    
    # Calculate hash
    fw_hash = hashlib.sha256(firmware_data).hexdigest()
    
    # Prepare payload
    payload = {
        "version": "1.0.4",
        "size": len(firmware_data),
        "hash": fw_hash,
        "chunk_size": 1024,
        "firmware_data": base64.b64encode(firmware_data).decode('ascii')
    }
    
    print(f"Uploading firmware 1.0.4 ({len(firmware_data)} bytes)")
    print(f"Hash: {fw_hash[:16]}...")
    
    # Upload to cloud
    try:
        response = requests.post("http://localhost:8080/api/cloud/fota/upload", 
                               json=payload, timeout=10)
        print(f"Response: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"Success! Total chunks: {result.get('total_chunks')}")
        else:
            print(f"Error: {response.text}")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    upload_firmware()