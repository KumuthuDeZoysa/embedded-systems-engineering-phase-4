
#pragma once
#include <stdint.h>
#include <vector>
#include <string>

struct LoggingConfig {
    std::string log_level;
    std::string log_file;
    bool flush_on_write;
};

struct ModbusConfig {
    uint8_t slave_address;
    uint32_t timeout_ms;
    uint8_t max_retries;
    uint32_t retry_delay_ms;
};

struct ApiConfig {
    std::string inverter_base_url;
    std::string upload_base_url;
    std::string read_endpoint;
    std::string write_endpoint;
    std::string config_endpoint;
    std::string upload_endpoint;
    std::string api_key;
};

struct RegisterConfig {
    uint8_t addr;
    std::string name;
    std::string unit;
    float gain;
    std::string access;
};

struct AcquisitionConfig {
    uint32_t polling_interval_ms;
    std::vector<uint8_t> minimum_registers;
    bool background_polling;
};

class ConfigManager {
public:
    ConfigManager(const char* config_file = "/config/config.json");
    ~ConfigManager();

    ModbusConfig getModbusConfig() const;
    ApiConfig getApiConfig() const;
    RegisterConfig getRegisterConfig(uint8_t addr) const;
    AcquisitionConfig getAcquisitionConfig() const;
    LoggingConfig getLoggingConfig() const;


private:
    ModbusConfig modbus_config_;
    ApiConfig api_config_;
    std::vector<RegisterConfig> register_configs_;
    AcquisitionConfig acquisition_config_;
    LoggingConfig logging_config_;
    void loadConfig(const char* config_file);
};
