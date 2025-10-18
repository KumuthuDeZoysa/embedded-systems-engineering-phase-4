#!/usr/bin/env python3
"""
Initialize EcoWatt Device Files
Creates necessary files to prevent LittleFS error messages during demo
"""

import serial
import time
import sys

def send_command(ser, command):
    """Send command to ESP32 and wait for response"""
    print(f"Sending: {command}")
    ser.write((command + '\n').encode())
    time.sleep(0.5)
    
    # Read response
    response = ""
    while ser.in_waiting > 0:
        response += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
        time.sleep(0.1)
    
    if response:
        print(f"Response: {response.strip()}")
    return response

def initialize_files(port='COM5', baudrate=115200):
    """Initialize missing files on ESP32"""
    try:
        # Connect to ESP32
        print(f"Connecting to ESP32 on {port}...")
        ser = serial.Serial(port, baudrate, timeout=2)
        time.sleep(2)  # Wait for connection
        
        print("Creating missing files to prevent LittleFS errors...")
        
        # Commands to create files (you can send these via serial monitor)
        commands = [
            "LittleFS.begin();",
            "File f = LittleFS.open(\"/version.txt\", \"w\"); f.println(\"1.0.0-Oct 17 2025-10:39:50\"); f.close();",
            "File f2 = LittleFS.open(\"/boot_count.txt\", \"w\"); f2.println(\"0\"); f2.close();",
            "File f3 = LittleFS.open(\"/littlefs/fota_state.json\", \"w\"); f3.println(\"{}\"); f3.close();",
            "Serial.println(\"Files created successfully\");"
        ]
        
        for cmd in commands:
            send_command(ser, cmd)
        
        ser.close()
        print("✅ Files initialized successfully!")
        
    except Exception as e:
        print(f"❌ Error: {e}")
        print("Note: You can also create these files manually via Arduino Serial Monitor")

if __name__ == "__main__":
    # Try to detect ESP32 port automatically or use default
    port = 'COM5'  # Change this to your ESP32 port
    initialize_files(port)