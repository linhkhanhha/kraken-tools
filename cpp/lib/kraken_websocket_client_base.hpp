#ifndef KRAKEN_WEBSOCKET_CLIENT_BASE_HPP
#define KRAKEN_WEBSOCKET_CLIENT_BASE_HPP

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <fstream>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include "kraken_common.hpp"
#include "flush_segment_mixin.hpp"

namespace kraken {

// Configuration constants
namespace {
    constexpr size_t MAX_LOGGED_FLUSHES = 3;  // Reduce log spam after N flushes
    constexpr size_t RECORD_BUFFER_INITIAL_CAPACITY = 1000;  // ~30s at 30 updates/sec
}

/**
 * Base template class for WebSocket clients with different JSON parsers
 *
 * Template parameter JsonParser must provide:
 * - static std::string build_subscription(const std::vector<std::string>& symbols)
 * - static void parse_message(const std::string& payload,
 *                             std::function<void(const TickerRecord&)> callback)
 *
 * This eliminates code duplication between nlohmann and simdjson implementations
 *
 * Inherits from FlushSegmentMixin for periodic flushing and segmentation
 */
template<typename JsonParser>
class KrakenWebSocketClientBase : public FlushSegmentMixin<KrakenWebSocketClientBase<JsonParser>> {
    friend class FlushSegmentMixin<KrakenWebSocketClientBase<JsonParser>>;  // Allow mixin to access private interface

public:
    // Type definitions
    using UpdateCallback = std::function<void(const TickerRecord&)>;
    using ConnectionCallback = std::function<void(bool connected)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    KrakenWebSocketClientBase();
    virtual ~KrakenWebSocketClientBase();

    // Disable copy
    KrakenWebSocketClientBase(const KrakenWebSocketClientBase&) = delete;
    KrakenWebSocketClientBase& operator=(const KrakenWebSocketClientBase&) = delete;

    // Public API
    bool start(std::vector<std::string> symbols);
    void stop();
    bool is_connected() const;
    bool is_running() const;

    /**
     * Get pending updates (polling pattern)
     *
     * Returns all updates received since the last call to get_updates().
     * The internal buffer is cleared after this call.
     *
     * IMPORTANT: If using periodic flushing (set_output_file + set_flush_interval),
     * you must call get_updates() more frequently than the flush_interval to avoid
     * data loss. During periodic flush, pending_updates_ is automatically cleared
     * to prevent memory leaks in callback-driven mode.
     *
     * Recommended: Poll at least every (flush_interval / 2)
     *
     * Example:
     *   client.set_flush_interval(std::chrono::seconds(30));
     *   while (running) {
     *     std::this_thread::sleep_for(std::chrono::seconds(10));  // < 30s
     *     auto updates = client.get_updates();
     *     // process updates
     *   }
     *
     * @return Vector of all pending ticker records (moves data, clears internal buffer)
     */
    std::vector<TickerRecord> get_updates();

    /**
     * Get full history of all ticker records
     * WARNING: This performs a deep copy of the entire history vector
     * For large datasets (long-running sessions), this can be expensive
     * Consider using pending_count() or get_updates() for frequent polling
     */
    std::vector<TickerRecord> get_history() const;

    size_t pending_count() const;
    void set_update_callback(UpdateCallback callback);
    void set_connection_callback(ConnectionCallback callback);
    void set_error_callback(ErrorCallback callback);

    /**
     * Flush remaining buffered data to configured output file
     * Use this after calling set_output_file() to ensure all data is written
     * NOTE: Does nothing if no output file is configured
     */
    void flush();

    /**
     * Save all historical data to a specific file (one-shot snapshot)
     * This creates a new file with all data and header, regardless of set_output_file()
     * Use this for ad-hoc exports or when not using periodic flushing
     * @param filename Target file to write snapshot
     */
    void save_to_csv(const std::string& filename);

    /**
     * Set output file for periodic flushing
     * NOTE: Should be called BEFORE start() to avoid race conditions
     * File I/O operations are performed under data_mutex_
     */
    void set_output_file(const std::string& filename);

    // Note: Flush/segment configuration methods inherited from FlushSegmentMixin:
    // - void set_flush_interval(std::chrono::seconds interval)
    // - void set_memory_threshold(size_t bytes)
    // - void set_segment_mode(SegmentMode mode)
    // - size_t get_flush_count() const
    // - size_t get_segment_count() const
    // - std::string get_current_segment_filename() const
    // - size_t get_current_memory_usage() const

protected:
    // WebSocket types
    typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
    typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

    // WebSocket client and connection
    client ws_client_;
    websocketpp::connection_hdl connection_hdl_;
    std::thread worker_thread_;

    // State
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::vector<std::string> symbols_;

    // Data storage (protected by data_mutex_)
    mutable std::mutex data_mutex_;
    std::vector<TickerRecord> ticker_history_;

    // pending_updates is used for polling pattern when there is no update_callback_ defined
    std::vector<TickerRecord> pending_updates_;

    // Output file configuration (protected by data_mutex_)
    // Note: output_filename_ and current_segment_filename_ (from mixin) relationship:
    // - Non-segmented mode: both point to same file
    // - Segmented mode: current_segment_filename_ has timestamp suffix, output_filename_ updated on transition
    std::string output_filename_;  // Base filename or current segment filename
    std::ofstream output_file_;    // Persistent file handle (unified with jsonl_writer approach)
    bool csv_header_written_;

    // Note: Flush/segment configuration inherited from FlushSegmentMixin:
    // - flush_interval_, memory_threshold_bytes_
    // - segment_mode_, segment_count_
    // - base_filename_, current_segment_filename_, current_segment_key_
    // - last_flush_time_, flush_count_

    // Callbacks (protected by callback_mutex_)
    mutable std::mutex callback_mutex_;
    UpdateCallback update_callback_;
    ConnectionCallback connection_callback_;
    ErrorCallback error_callback_;

    // WebSocket event handlers
    context_ptr on_tls_init(websocketpp::connection_hdl hdl);
    void on_open(websocketpp::connection_hdl hdl);
    void on_close(websocketpp::connection_hdl hdl);
    void on_fail(websocketpp::connection_hdl hdl);
    void on_message(websocketpp::connection_hdl hdl, client::message_ptr msg);

    // Worker thread main function
    void run_client();

    // Helper methods
    void notify_connection(bool connected);
    void notify_error(const std::string& error);
    void add_record(const TickerRecord& record);

private:
    // ========================================================================
    // CRTP Interface Implementation (required by FlushSegmentMixin)
    // ========================================================================

    /**
     * Get buffer size (required by CRTP)
     */
    size_t get_buffer_size() const {
        return ticker_history_.size();
    }

    /**
     * Get record size (required by CRTP)
     */
    size_t get_record_size() const {
        return sizeof(TickerRecord);
    }

    /**
     * Get file extension (required by CRTP)
     */
    std::string get_file_extension() const {
        return ".csv";
    }

    /**
     * Perform actual flush to disk (required by CRTP)
     */
    void perform_flush();

    /**
     * Perform segment transition (required by CRTP)
     */
    void perform_segment_transition(const std::string& new_filename);

    /**
     * Called when segmentation mode is set (required by CRTP)
     */
    void on_segment_mode_set();
};

// Implementation must be in header for templates

template<typename JsonParser>
KrakenWebSocketClientBase<JsonParser>::KrakenWebSocketClientBase()
    : FlushSegmentMixin<KrakenWebSocketClientBase<JsonParser>>(),  // Initialize mixin
      running_(false), connected_(false),
      csv_header_written_(false) {
    // Note: flush_interval_, memory_threshold_bytes_, flush_count_,
    // segment_mode_, segment_count_, last_flush_time_ are initialized by mixin
}

template<typename JsonParser>
KrakenWebSocketClientBase<JsonParser>::~KrakenWebSocketClientBase() {
    stop();

    // Flush and close output file if open
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (!ticker_history_.empty()) {
        this->force_flush();
    }
    if (output_file_.is_open()) {
        output_file_.close();
    }
}

template<typename JsonParser>
bool KrakenWebSocketClientBase<JsonParser>::start(std::vector<std::string> symbols) {
    if (running_) {
        std::cerr << "Client already running" << std::endl;
        return false;
    }

    if (symbols.empty()) {
        std::cerr << "No symbols provided" << std::endl;
        return false;
    }

    symbols_ = std::move(symbols);
    running_ = true;

    worker_thread_ = std::thread([this]() {
        this->run_client();
    });

    std::cout << "WebSocket client started (" << JsonParser::name() << " version)" << std::endl;
    return true;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    connected_ = false;

    try {
        if (!ws_client_.stopped()) {
            ws_client_.stop();
        }

        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }

        std::cout << "WebSocket client stopped" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error stopping client: " << e.what() << std::endl;
    }
}

template<typename JsonParser>
bool KrakenWebSocketClientBase<JsonParser>::is_connected() const {
    return connected_.load();
}

template<typename JsonParser>
bool KrakenWebSocketClientBase<JsonParser>::is_running() const {
    return running_.load();
}

template<typename JsonParser>
std::vector<TickerRecord> KrakenWebSocketClientBase<JsonParser>::get_updates() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    std::vector<TickerRecord> updates = std::move(pending_updates_);
    pending_updates_.clear();
    return updates;
}

template<typename JsonParser>
std::vector<TickerRecord> KrakenWebSocketClientBase<JsonParser>::get_history() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return ticker_history_;
}

template<typename JsonParser>
size_t KrakenWebSocketClientBase<JsonParser>::pending_count() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return pending_updates_.size();
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::set_update_callback(UpdateCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    update_callback_ = callback;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::set_connection_callback(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    connection_callback_ = callback;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::set_error_callback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = callback;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::flush() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    if (output_filename_.empty()) {
        std::cerr << "[Warning] flush() called but no output file configured. Use set_output_file() first." << std::endl;
        return;
    }

    if (ticker_history_.empty()) {
        return;  // Nothing to flush
    }

    // Force flush remaining buffered data
    this->force_flush();
    std::cout << "\nFinal flush completed." << std::endl;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::save_to_csv(const std::string& filename) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    // Always create a one-shot snapshot to the specified file
    // This is independent of the configured output_file_ for periodic flushing
    Utils::save_to_csv(filename, ticker_history_);
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::set_output_file(const std::string& filename) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    output_filename_ = filename;
    this->set_base_filename(filename);  // Update mixin's base filename

    // If segmentation is not enabled, open the file immediately
    // (For segmented mode, file opens when set_segment_mode is called)
    if (this->segment_mode_ == SegmentMode::NONE && !filename.empty()) {
        // Close any previously open file
        if (output_file_.is_open()) {
            output_file_.close();
        }

        // Open new file
        output_file_.open(filename, std::ios::out);
        if (!output_file_.is_open()) {
            std::cerr << "[Error] Cannot open output file: " << filename << std::endl;
        } else {
            csv_header_written_ = false;  // New file needs header
            this->current_segment_filename_ = filename;
        }
    }
}

template<typename JsonParser>
typename KrakenWebSocketClientBase<JsonParser>::context_ptr
KrakenWebSocketClientBase<JsonParser>::on_tls_init(websocketpp::connection_hdl) {
    context_ptr ctx = std::make_shared<boost::asio::ssl::context>(
        boost::asio::ssl::context::sslv23);

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                        boost::asio::ssl::context::no_sslv2 |
                        boost::asio::ssl::context::no_sslv3 |
                        boost::asio::ssl::context::single_dh_use);
    } catch (const std::exception& e) {
        notify_error("TLS init error: " + std::string(e.what()));
    }

    return ctx;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::on_open(websocketpp::connection_hdl hdl) {
    std::cout << "WebSocket connection opened" << std::endl;
    connection_hdl_ = hdl;
    connected_ = true;

    notify_connection(true);

    // Build subscription using parser-specific method
    std::string msg_str = JsonParser::build_subscription(symbols_);
    std::cout << "Subscribing to: " << msg_str << std::endl;

    try {
        ws_client_.send(hdl, msg_str, websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        notify_error("Send error: " + std::string(e.what()));
    }
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::on_close(websocketpp::connection_hdl) {
    std::cout << "WebSocket connection closed" << std::endl;
    connected_ = false;
    notify_connection(false);
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::on_fail(websocketpp::connection_hdl) {
    std::cerr << "WebSocket connection failed" << std::endl;
    connected_ = false;
    notify_connection(false);
    notify_error("Connection failed");
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::on_message(
    websocketpp::connection_hdl, client::message_ptr msg) {

    try {
        // Use parser-specific parsing - it will call add_record() for each ticker
        JsonParser::parse_message(msg->get_payload(),
            [this](const TickerRecord& record) {
                this->add_record(record);
            });
    } catch (const std::exception& e) {
        notify_error("Message handling error: " + std::string(e.what()));
    }
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::run_client() {
    try {
        ws_client_.init_asio();
        ws_client_.set_tls_init_handler([this](websocketpp::connection_hdl hdl) {
            return this->on_tls_init(hdl);
        });

        ws_client_.set_open_handler([this](websocketpp::connection_hdl hdl) {
            this->on_open(hdl);
        });
        ws_client_.set_message_handler([this](websocketpp::connection_hdl hdl, client::message_ptr msg) {
            this->on_message(hdl, msg);
        });
        ws_client_.set_close_handler([this](websocketpp::connection_hdl hdl) {
            this->on_close(hdl);
        });
        ws_client_.set_fail_handler([this](websocketpp::connection_hdl hdl) {
            this->on_fail(hdl);
        });

        // Set logging
        ws_client_.clear_access_channels(websocketpp::log::alevel::all);
        ws_client_.set_access_channels(websocketpp::log::alevel::connect);
        ws_client_.set_access_channels(websocketpp::log::alevel::disconnect);

        // Connect
        std::string uri = "wss://ws.kraken.com/v2";
        websocketpp::lib::error_code ec;
        client::connection_ptr con = ws_client_.get_connection(uri, ec);

        if (ec) {
            notify_error("Connection error: " + ec.message());
            running_ = false;
            return;
        }

        ws_client_.connect(con);
        std::cout << "Connecting to " << uri << "..." << std::endl;

        ws_client_.run();

    } catch (const websocketpp::exception& e) {
        notify_error("WebSocket++ exception: " + std::string(e.what()));
    } catch (const std::exception& e) {
        notify_error("Exception: " + std::string(e.what()));
    }

    running_ = false;
    connected_ = false;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::notify_connection(bool connected) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (connection_callback_) {
        connection_callback_(connected);
    }
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::notify_error(const std::string& error) {
    std::cerr << "[Error] " << error << std::endl;

    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (error_callback_) {
        error_callback_(error);
    }
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::add_record(const TickerRecord& record) {
    // Store in history and pending, check if flush needed
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        ticker_history_.push_back(record);
        pending_updates_.push_back(record);

        // CRTP: Single call handles everything automatically
        // - Segment transition detection
        // - Flush before segment transition
        // - Regular periodic flush
        // - Statistics tracking
        this->check_and_flush();
    }

    // Call user callback (outside data lock)
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (update_callback_) {
            update_callback_(record);
        }
    }
}

// ============================================================================
// CRTP Interface Implementation
// ============================================================================

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::perform_flush() {
    // Must be called with data_mutex_ held!

    if (ticker_history_.empty()) {
        return;
    }

    // Don't flush if no output file configured or file not open
    if (!output_file_.is_open()) {
        return;
    }

    // Write header only on first write
    if (!csv_header_written_) {
        output_file_ << "timestamp,pair,type,bid,bid_qty,ask,ask_qty,last,volume,vwap,low,high,change,change_pct\n";
        csv_header_written_ = true;
    }

    // Write data
    for (const auto& record : ticker_history_) {
        output_file_ << record.timestamp << ","
                     << record.pair << ","
                     << record.type << ","
                     << record.bid << ","
                     << record.bid_qty << ","
                     << record.ask << ","
                     << record.ask_qty << ","
                     << record.last << ","
                     << record.volume << ","
                     << record.vwap << ","
                     << record.low << ","
                     << record.high << ","
                     << record.change << ","
                     << record.change_pct << "\n";
    }

    // Flush to disk
    output_file_.flush();

    // Log flush (quiet output, only show on first few flushes)
    if (this->get_flush_count() < MAX_LOGGED_FLUSHES) {
        std::string target_filename = (this->segment_mode_ == SegmentMode::NONE) ?
                                      output_filename_ : this->current_segment_filename_;
        std::cout << "[FLUSH] Wrote " << ticker_history_.size() << " records to " << target_filename << std::endl;
    }

    // Clear buffers
    ticker_history_.clear();
    ticker_history_.reserve(RECORD_BUFFER_INITIAL_CAPACITY);

    // Also clear pending_updates_ to prevent memory leak in callback-driven mode
    // NOTE: If using polling pattern, call get_updates() more frequently than
    //       flush_interval_ to avoid data loss. See get_updates() documentation.
    pending_updates_.clear();
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::perform_segment_transition(const std::string& new_filename) {
    // Must be called with data_mutex_ held!

    // Close current file
    if (output_file_.is_open()) {
        output_file_.close();
    }

    // Mark that new file needs header
    csv_header_written_ = false;

    // Update output filename
    output_filename_ = new_filename;

    // Open new segment file (use 'out' only to overwrite, not append)
    output_file_.open(new_filename, std::ios::out);

    if (!output_file_.is_open()) {
        std::cerr << "[Error] Cannot open segment file: " << new_filename << std::endl;
    }

    std::cout << "[SEGMENT] Starting new file: " << new_filename << std::endl;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::on_segment_mode_set() {
    // NOTE: Called from FlushSegmentMixin::set_segment_mode() WITHOUT lock
    // We must acquire data_mutex_ here to protect file operations
    std::lock_guard<std::mutex> lock(data_mutex_);

    // Create first segment file when segmentation is enabled (unified with jsonl_writer)
    perform_segment_transition(this->current_segment_filename_);
}

} // namespace kraken

#endif // KRAKEN_WEBSOCKET_CLIENT_BASE_HPP
