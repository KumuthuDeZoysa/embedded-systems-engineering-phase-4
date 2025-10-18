#!/usr/bin/env python3
"""
Pre-Demo Verification Script
===========================

This script verifies that all Milestone 4 components are working correctly
before recording the demonstration video.

Run this script before filming to ensure everything is ready!
"""

import requests
import time
import json
from datetime import datetime

# Configuration
CLOUD_URL = "http://10.50.126.183:8080"
DEVICE_ID = "EcoWatt001"

class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    BOLD = '\033[1m'
    END = '\033[0m'

def print_header(title):
    print(f"\n{Colors.BLUE}{'='*60}{Colors.END}")
    print(f"{Colors.BLUE}{Colors.BOLD}{title.center(60)}{Colors.END}")
    print(f"{Colors.BLUE}{'='*60}{Colors.END}\n")

def check_mark(passed):
    return f"{Colors.GREEN}✅{Colors.END}" if passed else f"{Colors.RED}❌{Colors.END}"

def test_cloud_connectivity():
    """Test 1: Cloud server connectivity"""
    print_header("TEST 1: Cloud Server Connectivity")
    
    try:
        response = requests.get(f"{CLOUD_URL}/health", timeout=5)
        if response.status_code == 200:
            print(f"{check_mark(True)} Cloud server is responding at {CLOUD_URL}")
            return True
        else:
            print(f"{check_mark(False)} Cloud server error: {response.status_code}")
            return False
    except Exception as e:
        print(f"{check_mark(False)} Cannot connect to cloud server: {e}")
        print(f"{Colors.YELLOW}💡 Make sure Flask server is running:{Colors.END}")
        print(f"   cd \"E:\\ES Phase 4\\embedded-systems-engineering-phase-4\"")
        print(f"   python cpp-esp\\app.py")
        return False

def test_fota_endpoints():
    """Test 2: FOTA endpoints"""
    print_header("TEST 2: FOTA Endpoints")
    
    tests_passed = 0
    total_tests = 3
    
    # Test manifest endpoint
    try:
        response = requests.get(f"{CLOUD_URL}/api/inverter/fota/manifest")
        if response.status_code == 200:
            print(f"{check_mark(True)} FOTA manifest endpoint working")
            tests_passed += 1
        else:
            print(f"{check_mark(False)} FOTA manifest endpoint failed: {response.status_code}")
    except Exception as e:
        print(f"{check_mark(False)} FOTA manifest endpoint error: {e}")
    
    # Test status endpoint
    try:
        response = requests.get(f"{CLOUD_URL}/api/cloud/fota/status")
        if response.status_code == 200:
            print(f"{check_mark(True)} FOTA status endpoint working")
            tests_passed += 1
        else:
            print(f"{check_mark(False)} FOTA status endpoint failed: {response.status_code}")
    except Exception as e:
        print(f"{check_mark(False)} FOTA status endpoint error: {e}")
    
    # Test upload endpoint (with dummy data)
    try:
        dummy_payload = {
            "version": "test",
            "size": 100,
            "hash": "dummy_hash",
            "chunk_size": 1024,
            "firmware_data": "dGVzdA=="  # base64 for "test"
        }
        response = requests.post(f"{CLOUD_URL}/api/cloud/fota/upload", 
                               json=dummy_payload, timeout=10)
        if response.status_code in [200, 400]:  # 400 is expected for bad hash
            print(f"{check_mark(True)} FOTA upload endpoint working")
            tests_passed += 1
        else:
            print(f"{check_mark(False)} FOTA upload endpoint failed: {response.status_code}")
    except Exception as e:
        print(f"{check_mark(False)} FOTA upload endpoint error: {e}")
    
    print(f"\nFOTA Endpoints: {tests_passed}/{total_tests} working")
    return tests_passed == total_tests

def test_config_endpoints():
    """Test 3: Configuration endpoints"""
    print_header("TEST 3: Configuration Endpoints")
    
    tests_passed = 0
    total_tests = 2
    
    # Test config polling endpoint
    try:
        response = requests.get(f"{CLOUD_URL}/api/inverter/config")
        if response.status_code == 200:
            print(f"{check_mark(True)} Config polling endpoint working")
            tests_passed += 1
        else:
            print(f"{check_mark(False)} Config polling endpoint failed: {response.status_code}")
    except Exception as e:
        print(f"{check_mark(False)} Config polling endpoint error: {e}")
    
    # Test config status endpoint  
    try:
        response = requests.get(f"{CLOUD_URL}/api/config/status")
        if response.status_code == 200:
            print(f"{check_mark(True)} Config status endpoint working")
            tests_passed += 1
        else:
            print(f"{check_mark(False)} Config status endpoint failed: {response.status_code}")
    except Exception as e:
        print(f"{check_mark(False)} Config status endpoint error: {e}")
    
    print(f"\nConfig Endpoints: {tests_passed}/{total_tests} working")
    return tests_passed == total_tests

def test_command_endpoints():
    """Test 4: Command execution endpoints"""
    print_header("TEST 4: Command Execution Endpoints")
    
    tests_passed = 0
    total_tests = 2
    
    # Test command queue endpoint
    try:
        dummy_command = {
            "device_id": DEVICE_ID,
            "action": "read", 
            "target_register": "1"
        }
        response = requests.post(f"{CLOUD_URL}/api/command/queue",
                               json=dummy_command, timeout=5)
        if response.status_code == 200:
            print(f"{check_mark(True)} Command queue endpoint working")
            tests_passed += 1
        else:
            print(f"{check_mark(False)} Command queue endpoint failed: {response.status_code}")
    except Exception as e:
        print(f"{check_mark(False)} Command queue endpoint error: {e}")
    
    # Test command results endpoint
    try:
        response = requests.get(f"{CLOUD_URL}/api/command/results")
        if response.status_code == 200:
            print(f"{check_mark(True)} Command results endpoint working")
            tests_passed += 1
        else:
            print(f"{check_mark(False)} Command results endpoint failed: {response.status_code}")
    except Exception as e:
        print(f"{check_mark(False)} Command results endpoint error: {e}")
    
    print(f"\nCommand Endpoints: {tests_passed}/{total_tests} working")
    return tests_passed == total_tests

def test_security_features():
    """Test 5: Security implementation"""
    print_header("TEST 5: Security Features")
    
    tests_passed = 0
    total_tests = 2
    
    # Test security logs endpoint
    try:
        response = requests.get(f"{CLOUD_URL}/api/cloud/logs/security")
        if response.status_code == 200:
            print(f"{check_mark(True)} Security logs endpoint working")
            tests_passed += 1
        else:
            print(f"{check_mark(False)} Security logs endpoint failed: {response.status_code}")
    except Exception as e:
        print(f"{check_mark(False)} Security logs endpoint error: {e}")
    
    # Check if security is enabled
    try:
        # Try to access a protected endpoint without authentication
        response = requests.post(f"{CLOUD_URL}/api/upload", 
                               json={"test": "data"}, timeout=5)
        if response.status_code == 401:
            print(f"{check_mark(True)} Security authentication is active (401 response)")
            tests_passed += 1
        elif response.status_code == 200:
            print(f"{check_mark(False)} Security may not be active (200 response)")
        else:
            print(f"{check_mark(True)} Security responding (status: {response.status_code})")
            tests_passed += 1
    except Exception as e:
        print(f"{check_mark(False)} Security test error: {e}")
    
    print(f"\nSecurity Features: {tests_passed}/{total_tests} working")
    return tests_passed == total_tests

def show_demo_readiness():
    """Show final demo readiness status"""
    print_header("DEMO READINESS SUMMARY")
    
    print(f"{Colors.GREEN}✅ Pre-demo verification complete!{Colors.END}")
    print()
    print(f"{Colors.BOLD}Ready to record? Follow these steps:{Colors.END}")
    print()
    print("1. 🔌 Connect ESP32 to computer")
    print("2. 📱 Upload latest firmware:")
    print("   cd \"e:\\ES Phase 4\\embedded-systems-engineering-phase-4\\cpp-esp\"")
    print("   pio run --target upload --upload-port COM5")
    print()
    print("3. 📺 Open serial monitor:")
    print("   pio device monitor --port COM5 --baud 115200")
    print()
    print("4. 🌐 Open config dashboard:")
    print("   Open config_dashboard.html in your browser")
    print()
    print("5. 🎬 Start recording and follow MILESTONE_4_COMPLETE_DEMO_SCRIPT.md")
    print()
    print(f"{Colors.YELLOW}💡 Demo scripts ready:{Colors.END}")
    print("   - fota_production_demo.py --demo normal --version 1.0.6")
    print("   - fota_production_demo.py --demo rollback --version 1.0.7")
    print("   - security_demo.py (for security testing)")

def main():
    """Main verification function"""
    print_header("🚀 Milestone 4 Pre-Demo Verification")
    print(f"Checking all systems before video recording...")
    print(f"Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    all_tests = [
        test_cloud_connectivity(),
        test_fota_endpoints(), 
        test_config_endpoints(),
        test_command_endpoints(),
        test_security_features()
    ]
    
    passed_tests = sum(all_tests)
    total_tests = len(all_tests)
    
    print_header("VERIFICATION RESULTS")
    print(f"Tests Passed: {Colors.BOLD}{passed_tests}/{total_tests}{Colors.END}")
    
    if passed_tests == total_tests:
        print(f"{Colors.GREEN}🎉 ALL SYSTEMS GO! Ready for demo recording!{Colors.END}")
        show_demo_readiness()
    else:
        print(f"{Colors.RED}⚠️  Some tests failed. Please fix issues before recording.{Colors.END}")
        print()
        print(f"{Colors.YELLOW}Common fixes:{Colors.END}")
        print("- Ensure Flask server is running on correct IP")
        print("- Check network connectivity")
        print("- Verify ESP32 is connected and programmed")
        print("- Restart Flask server if needed")

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print(f"\n{Colors.YELLOW}Verification interrupted by user{Colors.END}")
    except Exception as e:
        print(f"\n{Colors.RED}Verification error: {e}{Colors.END}")