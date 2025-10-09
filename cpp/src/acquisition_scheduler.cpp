/**
 * @file acquisition_scheduler.cpp
 * @brief Data acquisition scheduler implementation
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#include "acquisition_scheduler.hpp"
#include "compression.hpp"
#include "logger.hpp"
#include <chrono>
#include <algorithm>
#include <thread>
#include <iostream>
#include <iomanip>
#include <ctime>

namespace ecoWatt {

// SampleBuffer implementation
void SampleBuffer::push(const AcquisitionSample& sample) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    buffer_[head_] = sample;
    head_ = (head_ + 1) % CAPACITY;
    
    if (size_ < CAPACITY) {
        size_++;
    }
}

std::vector<AcquisitionSample> SampleBuffer::getAllSamples() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<AcquisitionSample> result;
    result.reserve(size_);
    
    if (size_ == 0) {
        return result;
    }
    
    // Calculate starting position (oldest sample)
    size_t start = (size_ == CAPACITY) ? head_ : 0;
    
    // Copy samples in chronological order
    for (size_t i = 0; i < size_; ++i) {
        size_t index = (start + i) % CAPACITY;
        result.push_back(buffer_[index]);
    }
    
    return result;
}

void SampleBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_ = 0;
    head_ = 0;
}

// Constructor
AcquisitionScheduler::AcquisitionScheduler(SharedPtr<ProtocolAdapter> protocol_adapter,
                                         const ConfigManager& config)
    : protocol_adapter_(protocol_adapter), 
      last_buffer_output_(std::chrono::system_clock::now()) {
    
    // Get configuration
    config_ = config.getAcquisitionConfig();
    minimum_registers_ = config_.minimum_registers;
    
    LOG_INFO("AcquisitionScheduler initialized with interval: {}ms", config_.polling_interval.count());
}

// Destructor
AcquisitionScheduler::~AcquisitionScheduler() {
    stopPolling();
    LOG_INFO("AcquisitionScheduler destroyed");
}

// Start polling
void AcquisitionScheduler::startPolling() {
    if (polling_active_.load()) {
        LOG_WARN("AcquisitionScheduler already polling");
        return;
    }
    
    stop_requested_ = false;
    polling_active_ = true;
    
    // Start background thread
    polling_thread_ = std::make_unique<std::thread>(&AcquisitionScheduler::pollingLoop, this);
    
    LOG_INFO("AcquisitionScheduler started polling");
}

// Stop polling
void AcquisitionScheduler::stopPolling() {
    stop_requested_ = true;
    polling_active_ = false;
    
    if (polling_thread_ && polling_thread_->joinable()) {
        polling_thread_->join();
    }
    
    LOG_INFO("AcquisitionScheduler stopped polling");
}

// Set polling interval
void AcquisitionScheduler::setPollingInterval(Duration interval) {
    config_.polling_interval = interval;
    LOG_INFO("Updated polling interval to: {}ms", interval.count());
}

// Set minimum registers
void AcquisitionScheduler::setMinimumRegisters(const std::vector<RegisterAddress>& registers) {
    minimum_registers_ = registers;
    config_.minimum_registers = registers;
}

// Configure registers
void AcquisitionScheduler::configureRegisters(const std::map<RegisterAddress, RegisterConfig>& register_configs) {
    register_configs_ = register_configs;
}

// Add sample callback
void AcquisitionScheduler::addSampleCallback(SampleCallback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    sample_callbacks_.push_back(callback);
}

// Add error callback
void AcquisitionScheduler::addErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    error_callbacks_.push_back(callback);
}

// Read single register
UniquePtr<AcquisitionSample> AcquisitionScheduler::readSingleRegister(RegisterAddress address) {
    try {
        auto values = protocol_adapter_->readRegisters(address, 1);
        if (values.empty()) {
            return nullptr;
        }
        
        RegisterValue value = values[0];
        
        auto it = register_configs_.find(address);
        std::string name = (it != register_configs_.end()) ? it->second.name : "Unknown";
        std::string unit = (it != register_configs_.end()) ? it->second.unit : "";
        double gain = (it != register_configs_.end()) ? it->second.gain : 1.0;
        
        // Per API docs, 'gain' is a scaling divisor (e.g., gain 10 => value / 10)
        double scaled = (gain != 0.0) ? static_cast<double>(value) / gain
                                       : static_cast<double>(value);

        auto sample = std::make_unique<AcquisitionSample>(
            std::chrono::system_clock::now(),
            address,
            name,
            value,
            scaled,
            unit
        );
        
        return sample;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to read register {}: {}", address, e.what());
        return nullptr;
    }
}

// Read multiple registers
std::vector<AcquisitionSample> AcquisitionScheduler::readMultipleRegisters(const std::vector<RegisterAddress>& addresses) {
    std::vector<AcquisitionSample> samples;
    
    for (auto address : addresses) {
        auto sample = readSingleRegister(address);
        if (sample) {
            samples.push_back(*sample);
        }
    }
    
    return samples;
}

// Perform write operation
bool AcquisitionScheduler::performWriteOperation(RegisterAddress register_address, RegisterValue value) {
    try {
        return protocol_adapter_->writeRegister(register_address, value);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to write register {}: {}", register_address, e.what());
        return false;
    }
}



// Reset statistics
void AcquisitionScheduler::resetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_ = AcquisitionStatistics{};
}

// Main polling loop
void AcquisitionScheduler::pollingLoop() {
    LOG_INFO("Polling loop started");
    
    while (!stop_requested_.load()) {
        try {
            performPollCycle();
            
            // Process circular buffer for periodic output
            processCircularBuffer();
            
        } catch (const std::exception& e) {
            LOG_ERROR("Error in polling cycle: {}", e.what());
            notifyError(e.what());
        }
        
        // Wait for next poll interval
        std::this_thread::sleep_for(config_.polling_interval);
    }
    
    LOG_INFO("Polling loop stopped");
}

// Perform poll cycle
void AcquisitionScheduler::performPollCycle() {
    std::vector<RegisterAddress> addresses_to_read;
    
    // Collect all configured register addresses
    for (const auto& pair : register_configs_) {
        addresses_to_read.push_back(pair.first);
    }
    
    // Add minimum registers if not already included
    for (auto addr : minimum_registers_) {
        if (std::find(addresses_to_read.begin(), addresses_to_read.end(), addr) == addresses_to_read.end()) {
            addresses_to_read.push_back(addr);
        }
    }
    
    // Read all registers
    auto samples = readMultipleRegisters(addresses_to_read);
    
    // Store samples and notify callbacks
    for (const auto& sample : samples) {
        storeSample(sample);
    }
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.total_polls++;
        statistics_.last_poll_time = std::chrono::system_clock::now();
        
        if (!samples.empty()) {
            statistics_.successful_polls++;
        } else {
            statistics_.failed_polls++;
            statistics_.last_error = "No samples acquired";
        }
    }
}

// Store sample
void AcquisitionScheduler::storeSample(const AcquisitionSample& sample) {
    // Store in circular buffer for periodic output
    circular_buffer_.push(sample);
    
    // Notify callbacks
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        for (const auto& callback : sample_callbacks_) {
            try {
                callback(sample);
            } catch (const std::exception& e) {
                LOG_ERROR("Error in sample callback: {}", e.what());
            }
        }
    }
}

// Notify error
void AcquisitionScheduler::notifyError(const std::string& error_message) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    for (const auto& callback : error_callbacks_) {
        try {
            callback(error_message);
        } catch (const std::exception& e) {
            LOG_ERROR("Error in error callback: {}", e.what());
        }
    }
}

// Group consecutive registers
std::vector<std::vector<RegisterAddress>> AcquisitionScheduler::groupConsecutiveRegisters(
    const std::vector<RegisterAddress>& addresses) {
    
    std::vector<std::vector<RegisterAddress>> groups;
    if (addresses.empty()) return groups;
    
    auto sorted_addresses = addresses;
    std::sort(sorted_addresses.begin(), sorted_addresses.end());
    
    std::vector<RegisterAddress> current_group;
    current_group.push_back(sorted_addresses[0]);
    
    for (size_t i = 1; i < sorted_addresses.size(); ++i) {
        if (sorted_addresses[i] == sorted_addresses[i-1] + 1) {
            current_group.push_back(sorted_addresses[i]);
        } else {
            groups.push_back(current_group);
            current_group.clear();
            current_group.push_back(sorted_addresses[i]);
        }
    }
    
    if (!current_group.empty()) {
        groups.push_back(current_group);
    }
    
    return groups;
}

// Process circular buffer - print and clear samples every 15 seconds
void AcquisitionScheduler::processCircularBuffer() {
    auto now = std::chrono::system_clock::now();
    auto time_since_last = std::chrono::duration_cast<Duration>(now - last_buffer_output_);
    
    if (time_since_last >= BUFFER_OUTPUT_INTERVAL) {
        auto samples = circular_buffer_.getAllSamples();
        
        if (!samples.empty()) {
            LOG_INFO("=== Buffer Output ({} samples) ===", samples.size());
            
            // Perform compression with statistics
            auto compression_stats = DeltaCompression::compressWithStats(samples);
            
            for (const auto& sample : samples) {
                // Convert timestamp to readable format
                auto time_t = std::chrono::system_clock::to_time_t(sample.timestamp);
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    sample.timestamp.time_since_epoch()) % 1000;
                
                // Save current stream state
                auto old_fill = std::cout.fill();
                auto old_width = std::cout.width();
                auto old_flags = std::cout.flags();
                
                std::cout << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
                         << "." << std::setfill('0') << std::setw(3) << ms.count();
                
                // Reset stream state immediately after milliseconds
                std::cout.fill(old_fill);
                std::cout.width(old_width);
                std::cout.flags(old_flags);
                
                std::cout << " | " << sample.register_name 
                         << " (0x" << std::hex << sample.register_address << std::dec << "): "
                         << sample.scaled_value << " " << sample.unit
                         << " (raw: " << sample.raw_value << ")" << std::endl;
            }
            
            LOG_INFO("=== End Buffer Output ===");
            
            // Print compression report
            DeltaCompression::printCompressionReport(compression_stats);
        }
        
        // Clear the buffer and update timestamp
        circular_buffer_.clear();
        last_buffer_output_ = now;
    }
}

} // namespace ecoWatt
