/**
 * @file compression.cpp
 * @brief Delta encoding compression implementation
 * @author EcoWatt Team
 * @date 2025-09-19
 */

#include "compression.hpp"
#include "logger.hpp"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cstring>

namespace ecoWatt {

std::vector<uint8_t> DeltaCompression::compress(const std::vector<AcquisitionSample>& samples) {
    if (samples.empty()) {
        return {};
    }

    std::vector<uint8_t> compressed;
    compressed.reserve(samples.size() * 8); // Rough estimate

    // Header: number of samples
    encodeVarint(samples.size(), compressed);

    // First sample is stored as-is (base values)
    const auto& first = samples[0];
    
    // Store first sample's timestamp (system clock ticks since epoch)
    auto first_time = first.timestamp.time_since_epoch().count();
    encodeVarint(static_cast<uint64_t>(first_time), compressed);
    
    // Store first sample's register address
    encodeVarint(first.register_address, compressed);
    
    // Store first sample's raw value
    encodeVarint(zigzagEncode(first.raw_value), compressed);
    
    // Store first sample's scaled value (as fixed-point)
    int64_t scaled_fixed = static_cast<int64_t>(first.scaled_value * 1000000); // 6 decimal places
    encodeVarint(zigzagEncode(scaled_fixed), compressed);

    // Store register name length and name
    encodeVarint(first.register_name.length(), compressed);
    for (char c : first.register_name) {
        compressed.push_back(static_cast<uint8_t>(c));
    }

    // Store unit length and unit
    encodeVarint(first.unit.length(), compressed);
    for (char c : first.unit) {
        compressed.push_back(static_cast<uint8_t>(c));
    }

    // Store deltas with RLE for remaining samples
    // First collect all deltas
    std::vector<int64_t> time_deltas, addr_deltas, raw_deltas, scaled_deltas;
    
    for (size_t i = 1; i < samples.size(); i++) {
        const auto& current = samples[i];
        const auto& previous = samples[i-1];

        // Calculate deltas
        auto curr_time = current.timestamp.time_since_epoch().count();
        auto prev_time = previous.timestamp.time_since_epoch().count();
        time_deltas.push_back(curr_time - prev_time);

        addr_deltas.push_back(static_cast<int64_t>(current.register_address) - static_cast<int64_t>(previous.register_address));

        raw_deltas.push_back(current.raw_value - previous.raw_value);

        int64_t curr_scaled_fixed = static_cast<int64_t>(current.scaled_value * 1000000);
        int64_t prev_scaled_fixed = static_cast<int64_t>(previous.scaled_value * 1000000);
        scaled_deltas.push_back(curr_scaled_fixed - prev_scaled_fixed);
    }

    // Encode delta arrays with RLE
    encodeRLEArray(time_deltas, compressed);
    encodeRLEArray(addr_deltas, compressed);
    encodeRLEArray(raw_deltas, compressed);
    encodeRLEArray(scaled_deltas, compressed);

    // Store string changes for remaining samples
    for (size_t i = 1; i < samples.size(); i++) {
        const auto& current = samples[i];
        const auto& previous = samples[i-1];

        // For strings, only store if different from previous
        if (current.register_name != previous.register_name) {
            compressed.push_back(1); // Flag: name changed
            encodeVarint(current.register_name.length(), compressed);
            for (char c : current.register_name) {
                compressed.push_back(static_cast<uint8_t>(c));
            }
        } else {
            compressed.push_back(0); // Flag: name same
        }

        if (current.unit != previous.unit) {
            compressed.push_back(1); // Flag: unit changed
            encodeVarint(current.unit.length(), compressed);
            for (char c : current.unit) {
                compressed.push_back(static_cast<uint8_t>(c));
            }
        } else {
            compressed.push_back(0); // Flag: unit same
        }
    }

    return compressed;
}

std::vector<AcquisitionSample> DeltaCompression::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return {};
    }

    std::vector<AcquisitionSample> samples;
    size_t offset = 0;

    // Read number of samples
    size_t count = static_cast<size_t>(decodeVarint(data, offset));
    samples.reserve(count);

    if (count == 0) {
        return samples;
    }

    // Read first sample
    AcquisitionSample first;
    
    // Timestamp
    uint64_t first_time = decodeVarint(data, offset);
    // Restore timestamp using the same duration type as system_clock
    first.timestamp = TimePoint(TimePoint::duration(first_time));
    
    // Register address
    first.register_address = static_cast<RegisterAddress>(decodeVarint(data, offset));
    
    // Raw value
    first.raw_value = static_cast<RegisterValue>(zigzagDecode(decodeVarint(data, offset)));
    
    // Scaled value
    int64_t scaled_fixed = zigzagDecode(decodeVarint(data, offset));
    first.scaled_value = static_cast<double>(scaled_fixed) / 1000000.0;

    // Register name
    size_t name_len = static_cast<size_t>(decodeVarint(data, offset));
    first.register_name.reserve(name_len);
    for (size_t i = 0; i < name_len; i++) {
        first.register_name += static_cast<char>(data[offset++]);
    }

    // Unit
    size_t unit_len = static_cast<size_t>(decodeVarint(data, offset));
    first.unit.reserve(unit_len);
    for (size_t i = 0; i < unit_len; i++) {
        first.unit += static_cast<char>(data[offset++]);
    }

    samples.push_back(first);

    // Decode RLE-compressed delta arrays
    std::vector<int64_t> time_deltas, addr_deltas, raw_deltas, scaled_deltas;
    decodeRLEArray(data, offset, time_deltas);
    decodeRLEArray(data, offset, addr_deltas);
    decodeRLEArray(data, offset, raw_deltas);
    decodeRLEArray(data, offset, scaled_deltas);

    // Reconstruct remaining samples using decoded deltas
    for (size_t i = 1; i < count; i++) {
        AcquisitionSample current;
        const auto& previous = samples[i-1];
        size_t delta_idx = i - 1; // Delta index for arrays

        // Apply timestamp delta
        auto prev_time = previous.timestamp.time_since_epoch().count();
        current.timestamp = TimePoint(TimePoint::duration(prev_time + time_deltas[delta_idx]));

        // Apply register address delta
        current.register_address = static_cast<RegisterAddress>(
            static_cast<int64_t>(previous.register_address) + addr_deltas[delta_idx]);

        // Apply raw value delta
        current.raw_value = static_cast<RegisterValue>(previous.raw_value + raw_deltas[delta_idx]);

        // Apply scaled value delta
        int64_t prev_scaled_fixed = static_cast<int64_t>(previous.scaled_value * 1000000);
        int64_t curr_scaled_fixed = prev_scaled_fixed + scaled_deltas[delta_idx];
        current.scaled_value = static_cast<double>(curr_scaled_fixed) / 1000000.0;

        // Decode register name
        if (data[offset++] == 1) { // Name changed
            size_t name_len = static_cast<size_t>(decodeVarint(data, offset));
            current.register_name.reserve(name_len);
            for (size_t j = 0; j < name_len; j++) {
                current.register_name += static_cast<char>(data[offset++]);
            }
        } else {
            current.register_name = previous.register_name;
        }

        // Decode unit
        if (data[offset++] == 1) { // Unit changed
            size_t unit_len = static_cast<size_t>(decodeVarint(data, offset));
            current.unit.reserve(unit_len);
            for (size_t j = 0; j < unit_len; j++) {
                current.unit += static_cast<char>(data[offset++]);
            }
        } else {
            current.unit = previous.unit;
        }

        samples.push_back(current);
    }

    return samples;
}

CompressionStats DeltaCompression::compressWithStats(const std::vector<AcquisitionSample>& samples) {
    CompressionStats stats;

    if (samples.empty()) {
        return stats;
    }

    // Calculate original size
    stats.original_size_bytes = sizeof(AcquisitionSample) * samples.size();
    
    // Measure compression time
    auto start_compress = std::chrono::high_resolution_clock::now();
    auto compressed_data = compress(samples);
    auto end_compress = std::chrono::high_resolution_clock::now();
    
    stats.compression_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_compress - start_compress);

    stats.compressed_size_bytes = compressed_data.size();
    
    if (stats.original_size_bytes > 0) {
        stats.compression_ratio = static_cast<double>(stats.compressed_size_bytes) / 
                                 static_cast<double>(stats.original_size_bytes);
    }

    // Measure decompression time and validate
    auto start_decompress = std::chrono::high_resolution_clock::now();
    auto decompressed_samples = decompress(compressed_data);
    auto end_decompress = std::chrono::high_resolution_clock::now();
    
    stats.decompression_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_decompress - start_decompress);

    // Print original data
    printSampleData(samples, "ORIGINAL DATA");
    
    // Print decompressed data
    printSampleData(decompressed_samples, "DECOMPRESSED DATA");

    // Validate decompressed data
    stats.validation_passed = validateSamples(samples, decompressed_samples);

    return stats;
}

void DeltaCompression::printCompressionReport(const CompressionStats& stats) {
    std::cout << "\n+- Compression Report ------------------------------------+" << std::endl;
    std::cout << "| Original size:     " << std::setw(8) << stats.original_size_bytes << " bytes" << std::setw(12) << "|" << std::endl;
    std::cout << "| Compressed size:   " << std::setw(8) << stats.compressed_size_bytes << " bytes" << std::setw(12) << "|" << std::endl;
    std::cout << "| Compression ratio: " << std::setw(8) << std::fixed << std::setprecision(3) 
              << stats.compression_ratio << std::setw(18) << "|" << std::endl;
    std::cout << "| Compression time:  " << std::setw(8) << stats.compression_time.count() / 1000.0 << " μs" << std::setw(14) << "|" << std::endl;
    std::cout << "| Decompression time:" << std::setw(8) << stats.decompression_time.count() / 1000.0 << " μs" << std::setw(14) << "|" << std::endl;
    std::cout << "| Validation:        " << std::setw(8) << (stats.validation_passed ? "PASS" : "FAIL") << std::setw(18) << "|" << std::endl;
    std::cout << "+-----------------------------------------------------+" << std::endl;
}

void DeltaCompression::printSampleData(const std::vector<AcquisitionSample>& samples, const std::string& title) {
    std::cout << "\n+- " << title << " (" << samples.size() << " samples) " << std::string(40 - title.length(), '-') << "+" << std::endl;
    
    for (size_t i = 0; i < samples.size(); i++) {
        const auto& sample = samples[i];
        
        // Convert timestamp to readable format
        auto time_t = std::chrono::system_clock::to_time_t(sample.timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            sample.timestamp.time_since_epoch()) % 1000;
        
        // Save current stream state
        auto old_fill = std::cout.fill();
        auto old_width = std::cout.width();
        auto old_flags = std::cout.flags();
        
        std::cout << "| " << std::setw(2) << i << ": " 
                  << std::put_time(std::localtime(&time_t), "%H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count();
        
        // Reset stream state
        std::cout.fill(old_fill);
        std::cout.width(old_width);
        std::cout.flags(old_flags);
        
        std::cout << " | " << sample.register_name 
                  << " (0x" << std::hex << sample.register_address << std::dec << ")"
                  << " | Raw: " << sample.raw_value 
                  << " | Scaled: " << std::fixed << std::setprecision(3) << sample.scaled_value 
                  << " " << sample.unit << " |" << std::endl;
    }
    
    std::cout << "+" << std::string(79, '-') << "+" << std::endl;
}

void DeltaCompression::encodeVarint(uint64_t value, std::vector<uint8_t>& output) {
    while (value >= 0x80) {
        output.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    output.push_back(static_cast<uint8_t>(value & 0x7F));
}

uint64_t DeltaCompression::decodeVarint(const std::vector<uint8_t>& data, size_t& offset) {
    uint64_t result = 0;
    int shift = 0;
    
    while (offset < data.size()) {
        uint8_t byte = data[offset++];
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
        
        if ((byte & 0x80) == 0) {
            break;
        }
        
        shift += 7;
        if (shift >= 64) {
            throw std::runtime_error("Invalid varint encoding");
        }
    }
    
    return result;
}

uint64_t DeltaCompression::zigzagEncode(int64_t delta) {
    return static_cast<uint64_t>((delta << 1) ^ (delta >> 63));
}

int64_t DeltaCompression::zigzagDecode(uint64_t encoded) {
    return static_cast<int64_t>((encoded >> 1) ^ (static_cast<uint64_t>(-static_cast<int64_t>(encoded & 1))));
}

bool DeltaCompression::validateSamples(const std::vector<AcquisitionSample>& original,
                                      const std::vector<AcquisitionSample>& decompressed) {
    if (original.size() != decompressed.size()) {
        std::cout << "Validation FAIL: Size mismatch - Original: " << original.size() 
                  << ", Decompressed: " << decompressed.size() << std::endl;
        return false;
    }

    for (size_t i = 0; i < original.size(); i++) {
        const auto& orig = original[i];
        const auto& decomp = decompressed[i];

        // Check timestamp with tolerance for precision loss
        auto orig_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(orig.timestamp.time_since_epoch()).count();
        auto decomp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(decomp.timestamp.time_since_epoch()).count();
        
        // Allow tolerance based on system clock precision (typically 100ns on Windows)
        const int64_t timestamp_tolerance_ns = 1000; // 1 microsecond tolerance
        int64_t timestamp_diff = std::abs(orig_ns - decomp_ns);
        
        if (timestamp_diff > timestamp_tolerance_ns) {
            std::cout << "Validation FAIL at sample " << i << ": Timestamp mismatch beyond tolerance" << std::endl;
            std::cout << "  Original:     " << orig_ns << " ns" << std::endl;
            std::cout << "  Decompressed: " << decomp_ns << " ns" << std::endl;
            std::cout << "  Difference:   " << timestamp_diff << " ns (tolerance: " << timestamp_tolerance_ns << " ns)" << std::endl;
            return false;
        }

        // Check register address
        if (orig.register_address != decomp.register_address) {
            std::cout << "Validation FAIL at sample " << i << ": Register address mismatch" << std::endl;
            std::cout << "  Original:    " << orig.register_address << std::endl;
            std::cout << "  Decompressed: " << decomp.register_address << std::endl;
            return false;
        }

        // Check raw value
        if (orig.raw_value != decomp.raw_value) {
            std::cout << "Validation FAIL at sample " << i << ": Raw value mismatch" << std::endl;
            std::cout << "  Original:    " << orig.raw_value << std::endl;
            std::cout << "  Decompressed: " << decomp.raw_value << std::endl;
            return false;
        }

        // Check scaled value with tolerance
        if (std::abs(orig.scaled_value - decomp.scaled_value) > 0.000001) {
            std::cout << "Validation FAIL at sample " << i << ": Scaled value mismatch" << std::endl;
            std::cout << "  Original:    " << std::fixed << std::setprecision(8) << orig.scaled_value << std::endl;
            std::cout << "  Decompressed: " << std::fixed << std::setprecision(8) << decomp.scaled_value << std::endl;
            std::cout << "  Difference:   " << std::abs(orig.scaled_value - decomp.scaled_value) << std::endl;
            return false;
        }

        // Check register name
        if (orig.register_name != decomp.register_name) {
            std::cout << "Validation FAIL at sample " << i << ": Register name mismatch" << std::endl;
            std::cout << "  Original:    '" << orig.register_name << "'" << std::endl;
            std::cout << "  Decompressed: '" << decomp.register_name << "'" << std::endl;
            return false;
        }

        // Check unit
        if (orig.unit != decomp.unit) {
            std::cout << "Validation FAIL at sample " << i << ": Unit mismatch" << std::endl;
            std::cout << "  Original:    '" << orig.unit << "'" << std::endl;
            std::cout << "  Decompressed: '" << decomp.unit << "'" << std::endl;
            return false;
        }
    }

    return true;
}

void DeltaCompression::encodeDeltaRLE(int64_t delta, size_t count, std::vector<uint8_t>& output) {
    // Encode zigzag delta
    uint64_t encoded_delta = zigzagEncode(delta);
    
    // RLE format: if count > 1, use RLE encoding
    if (count > 1) {
        // RLE marker: high bit set in first varint indicates RLE
        encodeVarint(encoded_delta | 0x8000000000000000ULL, output);
        encodeVarint(count, output);
    } else {
        // Single value: just encode the delta (high bit clear)
        encodeVarint(encoded_delta & 0x7FFFFFFFFFFFFFFFULL, output);
    }
}

void DeltaCompression::decodeDeltaRLE(const std::vector<uint8_t>& data, size_t& offset, int64_t& delta, size_t& count) {
    uint64_t first_varint = decodeVarint(data, offset);
    
    // Check RLE marker (high bit)
    if (first_varint & 0x8000000000000000ULL) {
        // RLE encoded: read count
        uint64_t encoded_delta = first_varint & 0x7FFFFFFFFFFFFFFFULL;
        count = static_cast<size_t>(decodeVarint(data, offset));
        delta = zigzagDecode(encoded_delta);
    } else {
        // Single value
        count = 1;
        delta = zigzagDecode(first_varint);
    }
}

void DeltaCompression::encodeRLEArray(const std::vector<int64_t>& deltas, std::vector<uint8_t>& output) {
    if (deltas.empty()) {
        // Encode empty array as count 0
        encodeVarint(0, output);
        return;
    }
    
    // Encode array size first
    encodeVarint(deltas.size(), output);
    
    // Apply RLE compression
    size_t i = 0;
    while (i < deltas.size()) {
        int64_t current_delta = deltas[i];
        size_t run_length = 1;
        
        // Count consecutive identical values
        while (i + run_length < deltas.size() && deltas[i + run_length] == current_delta) {
            run_length++;
        }
        
        // Encode the delta and run length
        encodeDeltaRLE(current_delta, run_length, output);
        i += run_length;
    }
}

void DeltaCompression::decodeRLEArray(const std::vector<uint8_t>& data, size_t& offset, std::vector<int64_t>& deltas) {
    // Decode array size
    size_t array_size = static_cast<size_t>(decodeVarint(data, offset));
    
    if (array_size == 0) {
        deltas.clear();
        return;
    }
    
    deltas.clear();
    deltas.reserve(array_size);
    
    // Decode RLE-compressed values
    while (deltas.size() < array_size) {
        int64_t delta;
        size_t count;
        decodeDeltaRLE(data, offset, delta, count);
        
        // Expand the run
        for (size_t j = 0; j < count && deltas.size() < array_size; ++j) {
            deltas.push_back(delta);
        }
    }
}

} // namespace ecoWatt