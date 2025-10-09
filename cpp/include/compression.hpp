/**
 * @file compression.hpp
 * @brief Delta encoding compression for acquisition samples
 * @author EcoWatt Team
 * @date 2025-09-19
 */

#pragma once

#include "types.hpp"
#include <vector>
#include <chrono>
#include <cstdint>

namespace ecoWatt {

/**
 * @brief Compression statistics for performance analysis
 */
struct CompressionStats {
    size_t original_size_bytes{0};
    size_t compressed_size_bytes{0};
    double compression_ratio{0.0};
    std::chrono::nanoseconds compression_time{0};
    std::chrono::nanoseconds decompression_time{0};
    bool validation_passed{false};
};

/**
 * @brief Delta encoding compression utilities
 */
class DeltaCompression {
public:
    /**
     * @brief Compress samples using delta encoding
     * @param samples Vector of acquisition samples to compress
     * @return Compressed data as byte vector
     */
    static std::vector<uint8_t> compress(const std::vector<AcquisitionSample>& samples);

    /**
     * @brief Decompress delta-encoded data back to samples
     * @param data Compressed byte data
     * @return Decompressed acquisition samples
     */
    static std::vector<AcquisitionSample> decompress(const std::vector<uint8_t>& data);

    /**
     * @brief Perform compression with full statistics and validation
     * @param samples Original samples to compress
     * @return Compression statistics including timing and validation
     */
    static CompressionStats compressWithStats(const std::vector<AcquisitionSample>& samples);

    /**
     * @brief Print compression statistics in formatted output
     * @param stats Compression statistics to display
     */
    static void printCompressionReport(const CompressionStats& stats);

    /**
     * @brief Print detailed sample data for debugging
     * @param samples Samples to print
     * @param title Title for the data section
     */
    static void printSampleData(const std::vector<AcquisitionSample>& samples, const std::string& title);

private:
    /**
     * @brief Encode a 64-bit value using variable-length encoding
     * @param value Value to encode
     * @param output Output byte vector
     */
    static void encodeVarint(uint64_t value, std::vector<uint8_t>& output);

    /**
     * @brief Decode a variable-length encoded value
     * @param data Input byte data
     * @param offset Current read position (updated after read)
     * @return Decoded 64-bit value
     */
    static uint64_t decodeVarint(const std::vector<uint8_t>& data, size_t& offset);

    /**
     * @brief Encode delta value with RLE compression
     * @param delta Delta value to encode
     * @param count Run length (1 if no run)
     * @param output Output byte vector
     */
    static void encodeDeltaRLE(int64_t delta, size_t count, std::vector<uint8_t>& output);

    /**
     * @brief Decode delta value with RLE decompression
     * @param data Input byte data
     * @param offset Current read position (updated after read)
     * @param delta Decoded delta value
     * @param count Decoded run length
     */
    static void decodeDeltaRLE(const std::vector<uint8_t>& data, size_t& offset, int64_t& delta, size_t& count);

    /**
     * @brief Encode an array of deltas using RLE compression
     * @param deltas Array of delta values
     * @param output Output byte vector
     */
    static void encodeRLEArray(const std::vector<int64_t>& deltas, std::vector<uint8_t>& output);

    /**
     * @brief Decode an RLE-compressed array of deltas
     * @param data Input byte data
     * @param offset Current read position (updated after read)
     * @param deltas Output vector for decoded deltas
     */
    static void decodeRLEArray(const std::vector<uint8_t>& data, size_t& offset, std::vector<int64_t>& deltas);

    /**
     * @brief Encode a signed delta using zigzag encoding
     * @param delta Signed delta value
     * @return Zigzag encoded unsigned value
     */
    static uint64_t zigzagEncode(int64_t delta);

    /**
     * @brief Decode a zigzag encoded value back to signed delta
     * @param encoded Zigzag encoded value
     * @return Original signed delta
     */
    static int64_t zigzagDecode(uint64_t encoded);

    /**
     * @brief Validate that decompressed samples match original
     * @param original Original samples
     * @param decompressed Decompressed samples
     * @return True if samples match exactly
     */
    static bool validateSamples(const std::vector<AcquisitionSample>& original,
                               const std::vector<AcquisitionSample>& decompressed);
};

} // namespace ecoWatt