/**
 * Flush and Segmentation Mixin (CRTP Pattern)
 *
 * Provides periodic flushing and time-based file segmentation capabilities
 * to any writer class using the Curiously Recurring Template Pattern (CRTP).
 *
 * Benefits:
 * - Zero runtime overhead (no virtual dispatch)
 * - Compile-time interface enforcement
 * - Type-safe static polymorphism
 * - Reusable across CSV, JSONL, and other writers
 *
 * Required Interface (enforced at compile time):
 * The derived class must implement:
 *
 *   size_t get_buffer_size() const
 *       Returns the number of records currently buffered
 *
 *   size_t get_record_size() const
 *       Returns the size in bytes of a single record
 *
 *   std::string get_file_extension() const
 *       Returns the file extension (e.g., ".csv", ".jsonl")
 *
 *   void perform_flush()
 *       Actually writes buffered data to disk
 *
 *   void perform_segment_transition(const std::string& new_filename)
 *       Closes current file and opens new segment file
 *
 *   void on_segment_mode_set()
 *       Called when segmentation mode is enabled (optional hook)
 *
 * Usage Example:
 *
 *   class MyWriter : public FlushSegmentMixin<MyWriter> {
 *       friend class FlushSegmentMixin<MyWriter>;
 *   private:
 *       size_t get_buffer_size() const { return buffer_.size(); }
 *       size_t get_record_size() const { return sizeof(Record); }
 *       std::string get_file_extension() const { return ".csv"; }
 *       void perform_flush() { ... }
 *       void perform_segment_transition(const std::string& fname) { ... }
 *       void on_segment_mode_set() { ... }
 *   public:
 *       void write_record(const Record& r) {
 *           buffer_.push_back(r);
 *           check_and_flush();  // All logic handled automatically
 *       }
 *   };
 */

#ifndef FLUSH_SEGMENT_MIXIN_HPP
#define FLUSH_SEGMENT_MIXIN_HPP

#include <chrono>
#include <string>
#include <iostream>
#include <ctime>
#include <iomanip>

namespace kraken {

/**
 * Segmentation mode for time-based file splitting
 */
enum class SegmentMode {
    NONE,    // Single file (default)
    HOURLY,  // One file per hour (YYYYMMDD_HH)
    DAILY    // One file per day (YYYYMMDD)
};

/**
 * CRTP Mixin for flush and segmentation management
 *
 * @tparam Derived The derived class type (CRTP pattern)
 */
template<typename Derived>
class FlushSegmentMixin {
protected:
    // ========================================================================
    // Configuration
    // ========================================================================
    std::chrono::seconds flush_interval_;          // Time-based flush trigger
    size_t memory_threshold_bytes_;                // Memory-based flush trigger
    SegmentMode segment_mode_;                     // Segmentation mode

    // ========================================================================
    // State
    // ========================================================================
    std::chrono::steady_clock::time_point last_flush_time_;
    size_t flush_count_;
    size_t segment_count_;
    std::string current_segment_key_;              // Current segment identifier (e.g., "20251112_10")
    std::string current_segment_filename_;         // Current segment filename
    std::string base_filename_;                    // Base filename without segment suffix

    /**
     * Constructor - initializes with default values
     */
    FlushSegmentMixin()
        : flush_interval_(30),                     // Default: 30 seconds
          memory_threshold_bytes_(10 * 1024 * 1024),  // Default: 10 MB
          segment_mode_(SegmentMode::NONE),
          flush_count_(0),
          segment_count_(0) {
        last_flush_time_ = std::chrono::steady_clock::now();
    }

    // Non-copyable
    FlushSegmentMixin(const FlushSegmentMixin&) = delete;
    FlushSegmentMixin& operator=(const FlushSegmentMixin&) = delete;

public:
    // ========================================================================
    // Configuration API
    // ========================================================================

    /**
     * Set flush interval (time-based trigger)
     * @param interval Flush interval in seconds (0 to disable)
     */
    void set_flush_interval(std::chrono::seconds interval) {
        flush_interval_ = interval;
    }

    /**
     * Set memory threshold (memory-based trigger)
     * @param bytes Memory threshold in bytes (0 to disable)
     */
    void set_memory_threshold(size_t bytes) {
        memory_threshold_bytes_ = bytes;
    }

    /**
     * Set base output filename
     * @param filename Base filename (without segment suffix)
     */
    void set_base_filename(const std::string& filename) {
        base_filename_ = filename;
    }

    /**
     * Set segmentation mode
     * @param mode NONE, HOURLY, or DAILY
     */
    void set_segment_mode(SegmentMode mode) {
        segment_mode_ = mode;

        if (mode != SegmentMode::NONE) {
            // Initialize first segment
            current_segment_key_ = generate_segment_key();
            current_segment_filename_ = insert_segment_key(
                base_filename_,
                current_segment_key_,
                derived()->get_file_extension()
            );

            // Notify derived class
            derived()->on_segment_mode_set();

            segment_count_ = 1;
            std::cout << "[SEGMENT] Starting new file: "
                     << current_segment_filename_ << std::endl;
        }
    }

    // ========================================================================
    // Getters
    // ========================================================================

    size_t get_flush_count() const {
        return flush_count_;
    }

    size_t get_segment_count() const {
        return segment_count_;
    }

    std::string get_current_segment_filename() const {
        return current_segment_filename_;
    }

    size_t get_current_memory_usage() const {
        return derived()->get_buffer_size() * derived()->get_record_size();
    }

protected:
    // ========================================================================
    // CRTP Helper
    // ========================================================================

    /**
     * Get pointer to derived class (non-const)
     */
    Derived* derived() {
        return static_cast<Derived*>(this);
    }

    /**
     * Get pointer to derived class (const)
     */
    const Derived* derived() const {
        return static_cast<const Derived*>(this);
    }

    // ========================================================================
    // Core Logic
    // ========================================================================

    /**
     * Check if flush should be triggered
     * Uses OR logic: flushes if time exceeded OR memory exceeded
     */
    bool should_flush() const {
        size_t buffer_size = derived()->get_buffer_size();
        if (buffer_size == 0) {
            return false;
        }

        // Time-based trigger
        bool time_exceeded = false;
        if (flush_interval_.count() > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - last_flush_time_
            );
            time_exceeded = (elapsed >= flush_interval_);
        }

        // Memory-based trigger
        bool memory_exceeded = false;
        if (memory_threshold_bytes_ > 0) {
            size_t memory_bytes = buffer_size * derived()->get_record_size();
            memory_exceeded = (memory_bytes >= memory_threshold_bytes_);
        }

        // OR logic: flush if either condition met
        return time_exceeded || memory_exceeded;
    }

    /**
     * Check if segment transition is needed
     */
    bool should_transition_segment() const {
        if (segment_mode_ == SegmentMode::NONE) {
            return false;
        }

        std::string new_key = generate_segment_key();
        return new_key != current_segment_key_;
    }

    /**
     * Generate segment key based on current time
     * Returns YYYYMMDD_HH for hourly, YYYYMMDD for daily
     */
    std::string generate_segment_key() const {
        if (segment_mode_ == SegmentMode::NONE) {
            return "";
        }

        auto now = std::time(nullptr);
        auto tm = *std::gmtime(&now);  // UTC

        char buffer[32];
        if (segment_mode_ == SegmentMode::HOURLY) {
            // Format: YYYYMMDD_HH
            std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H", &tm);
        } else if (segment_mode_ == SegmentMode::DAILY) {
            // Format: YYYYMMDD
            std::strftime(buffer, sizeof(buffer), "%Y%m%d", &tm);
        } else {
            return "";
        }

        return std::string(buffer);
    }

    /**
     * Insert segment key into filename before extension
     * E.g., "output.csv" + "20251112_10" -> "output.20251112_10.csv"
     */
    std::string insert_segment_key(const std::string& base,
                                   const std::string& key,
                                   const std::string& extension) const {
        // Find the extension position
        size_t ext_pos = base.rfind(extension);

        if (ext_pos == std::string::npos) {
            // Extension not found, append
            return base + "." + key + extension;
        }

        // Insert segment key before extension
        return base.substr(0, ext_pos) + "." + key + extension;
    }

public:
    // ========================================================================
    // Primary Interface - Call this from derived class
    // ========================================================================

    /**
     * Check and perform flush/segmentation if needed
     *
     * This is the ONLY method the derived class needs to call.
     * It handles:
     * - Segment transition detection
     * - Flush before segment transition
     * - Regular periodic flush
     * - Statistics tracking
     * - Console logging
     *
     * Usage: Call after adding each record to buffer
     */
    void check_and_flush() {
        // Check for segment transition first
        if (should_transition_segment()) {
            // Flush current buffer before transitioning
            if (derived()->get_buffer_size() > 0) {
                derived()->perform_flush();
                flush_count_++;
                last_flush_time_ = std::chrono::steady_clock::now();
            }

            // Transition to new segment
            std::string new_key = generate_segment_key();
            current_segment_key_ = new_key;
            current_segment_filename_ = insert_segment_key(
                base_filename_,
                new_key,
                derived()->get_file_extension()
            );

            derived()->perform_segment_transition(current_segment_filename_);
            segment_count_++;

            std::cout << "[SEGMENT] Starting new file: "
                     << current_segment_filename_ << std::endl;
        }

        // Check if regular flush needed
        if (should_flush()) {
            derived()->perform_flush();
            flush_count_++;
            last_flush_time_ = std::chrono::steady_clock::now();

            // Quiet mode after 3 flushes
            if (flush_count_ <= 3) {
                std::cout << "[FLUSH] Wrote records to "
                         << (segment_mode_ == SegmentMode::NONE ?
                             base_filename_ : current_segment_filename_)
                         << std::endl;
            }
        }
    }

    /**
     * Force immediate flush
     * Useful for shutdown/cleanup
     */
    void force_flush() {
        if (derived()->get_buffer_size() > 0) {
            derived()->perform_flush();
            flush_count_++;
            last_flush_time_ = std::chrono::steady_clock::now();
        }
    }
};

} // namespace kraken

#endif // FLUSH_SEGMENT_MIXIN_HPP
