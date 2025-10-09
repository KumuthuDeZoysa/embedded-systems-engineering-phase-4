# TODO for Fixing Outbox File Creation and Timestamp Issues

## Tasks
- [ ] Change outbox file path in uplink_packetizer.cpp from "/littlefs/outbox_%d" to "/outbox_%d" in drainOutbox_ and uploadTask functions.
- [ ] Verify that LittleFS is mounted correctly and files can be created at root level.
- [ ] Test the device to ensure outbox files are created and uploaded without errors.
- [ ] Confirm timestamp warnings are handled appropriately (delta set to 0 for non-increasing timestamps).
