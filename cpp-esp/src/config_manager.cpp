
#include "../include/config_manager.hpp"
#include <Arduino.h>



ConfigManager::ConfigManager(const char* config_file) {
    // Hardcoded Modbus config
    modbus_config_.slave_address = 17;
    modbus_config_.timeout_ms = 5000;
    modbus_config_.max_retries = 3;
    modbus_config_.retry_delay_ms = 1000;

    // Hardcoded API config (inverter data accessed via api_key only)
    api_config_.inverter_base_url = "http://20.15.114.131:8080";
    api_config_.read_endpoint = "/api/inverter/read";
    api_config_.write_endpoint = "/api/inverter/write";
    api_config_.config_endpoint = "/api/inverter/config";
    api_config_.upload_endpoint = "http://10.50.126.197:8080/api/upload";
    api_config_.api_key = "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTFkOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExMw==";

    // Set correct gain values for each register
    register_configs_.clear();
    register_configs_.push_back({0, "Vac1_L1_Phase_voltage", "V", 10.0f, "Read"});
    register_configs_.push_back({1, "Iac1_L1_Phase_current", "A", 10.0f, "Read"});
    register_configs_.push_back({2, "Fac1_L1_Phase_frequency", "Hz", 100.0f, "Read"});
    register_configs_.push_back({3, "Vpv1_PV1_input_voltage", "V", 10.0f, "Read"});
    register_configs_.push_back({4, "Vpv2_PV2_input_voltage", "V", 10.0f, "Read"});
    register_configs_.push_back({5, "Ipv1_PV1_input_current", "A", 10.0f, "Read"});
    register_configs_.push_back({6, "Ipv2_PV2_input_current", "A", 10.0f, "Read"});
    register_configs_.push_back({7, "Inverter_internal_temperature", "°C", 10.0f, "Read"});
    register_configs_.push_back({8, "Export_power_percentage", "%", 1.0f, "Read/Write"});
    register_configs_.push_back({9, "Pac_L_Inverter_output_power", "W", 1.0f, "Read"});


    // Hardcoded acquisition config
    acquisition_config_.polling_interval_ms = 5000; // 5 seconds
    acquisition_config_.minimum_registers = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    acquisition_config_.background_polling = true;

    // Hardcoded logging config
    logging_config_.log_level = "DEBUG";
    logging_config_.log_file = "/logs/main.log";
    logging_config_.flush_on_write = true;
}

ConfigManager::~ConfigManager() {}



ModbusConfig ConfigManager::getModbusConfig() const { return modbus_config_; }
ApiConfig ConfigManager::getApiConfig() const { return api_config_; }
RegisterConfig ConfigManager::getRegisterConfig(uint8_t addr) const {
    for (const auto& config : register_configs_) {
        if (config.addr == addr) return config;
    }
    return RegisterConfig{};
}

AcquisitionConfig ConfigManager::getAcquisitionConfig() const { return acquisition_config_; }


