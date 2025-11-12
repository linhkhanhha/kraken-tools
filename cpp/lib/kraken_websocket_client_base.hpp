#ifndef KRAKEN_WEBSOCKET_CLIENT_BASE_HPP
#define KRAKEN_WEBSOCKET_CLIENT_BASE_HPP

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include "kraken_common.hpp"

namespace kraken {

/**
 * Segment mode for output file management
 */
enum class SegmentMode {
    NONE,    // Single file (default)
    HOURLY,  // One file per hour
    DAILY    // One file per day
};

/**
 * Base template class for WebSocket clients with different JSON parsers
 *
 * Template parameter JsonParser must provide:
 * - static std::string build_subscription(const std::vector<std::string>& symbols)
 * - static void parse_message(const std::string& payload,
 *                             std::function<void(const TickerRecord&)> callback)
 *
 * This eliminates code duplication between nlohmann and simdjson implementations
 */
template<typename JsonParser>
class KrakenWebSocketClientBase {
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
    bool start(const std::vector<std::string>& symbols);
    void stop();
    bool is_connected() const;
    bool is_running() const;
    std::vector<TickerRecord> get_updates();
    std::vector<TickerRecord> get_history() const;
    size_t pending_count() const;
    void set_update_callback(UpdateCallback callback);
    void set_connection_callback(ConnectionCallback callback);
    void set_error_callback(ErrorCallback callback);
    void save_to_csv(const std::string& filename);

    // Flush configuration
    void set_flush_interval(std::chrono::seconds interval);
    void set_memory_threshold(size_t bytes);
    void set_output_file(const std::string& filename);
    void set_segment_mode(SegmentMode mode);

    // Flush statistics
    size_t get_flush_count() const;
    size_t get_current_memory_usage() const;
    std::string get_current_segment_filename() const;
    size_t get_segment_count() const;

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
    std::vector<TickerRecord> pending_updates_;

    // Flush configuration (protected by data_mutex_)
    std::chrono::seconds flush_interval_;
    size_t memory_threshold_bytes_;
    std::string output_filename_;
    std::chrono::steady_clock::time_point last_flush_time_;
    bool csv_header_written_;
    size_t flush_count_;

    // Segmentation configuration (protected by data_mutex_)
    SegmentMode segment_mode_;
    std::string base_output_filename_;
    std::string current_segment_filename_;
    std::string current_segment_key_;
    size_t segment_count_;

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

    // Flush helper methods
    bool should_flush() const;
    void flush_records_to_csv(const std::vector<TickerRecord>& records);

    // Segmentation helper methods
    std::string generate_segment_key() const;
    std::string generate_segment_filename() const;
    bool should_start_new_segment() const;
};

// Implementation must be in header for templates

template<typename JsonParser>
KrakenWebSocketClientBase<JsonParser>::KrakenWebSocketClientBase()
    : running_(false), connected_(false),
      flush_interval_(30),  // Default 30 seconds
      memory_threshold_bytes_(10 * 1024 * 1024),  // Default 10MB
      csv_header_written_(false),
      flush_count_(0),
      segment_mode_(SegmentMode::NONE),
      segment_count_(0) {
    last_flush_time_ = std::chrono::steady_clock::now();
}

template<typename JsonParser>
KrakenWebSocketClientBase<JsonParser>::~KrakenWebSocketClientBase() {
    stop();
}

template<typename JsonParser>
bool KrakenWebSocketClientBase<JsonParser>::start(const std::vector<std::string>& symbols) {
    if (running_) {
        std::cerr << "Client already running" << std::endl;
        return false;
    }

    if (symbols.empty()) {
        std::cerr << "No symbols provided" << std::endl;
        return false;
    }

    symbols_ = symbols;
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
void KrakenWebSocketClientBase<JsonParser>::save_to_csv(const std::string& filename) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    // If we have an output file configured and already wrote header,
    // this is a final flush - just append remaining data
    if (!output_filename_.empty() && csv_header_written_ && !ticker_history_.empty()) {
        flush_records_to_csv(ticker_history_);
        ticker_history_.clear();
        std::cout << "\nFinal flush completed." << std::endl;
    } else {
        // Traditional behavior: write all data with header
        Utils::save_to_csv(filename, ticker_history_);
    }
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::set_flush_interval(std::chrono::seconds interval) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    flush_interval_ = interval;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::set_memory_threshold(size_t bytes) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    memory_threshold_bytes_ = bytes;
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::set_output_file(const std::string& filename) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    output_filename_ = filename;
    base_output_filename_ = filename;  // Store base filename for segmentation
}

template<typename JsonParser>
size_t KrakenWebSocketClientBase<JsonParser>::get_flush_count() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return flush_count_;
}

template<typename JsonParser>
size_t KrakenWebSocketClientBase<JsonParser>::get_current_memory_usage() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return ticker_history_.size() * sizeof(TickerRecord);
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::set_segment_mode(SegmentMode mode) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    segment_mode_ = mode;
}

template<typename JsonParser>
std::string KrakenWebSocketClientBase<JsonParser>::get_current_segment_filename() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_segment_filename_.empty() ? output_filename_ : current_segment_filename_;
}

template<typename JsonParser>
size_t KrakenWebSocketClientBase<JsonParser>::get_segment_count() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return segment_count_;
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
    } catch (std::exception& e) {
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
    std::vector<TickerRecord> records_to_flush;

    // Store in history and pending, check if flush needed
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        ticker_history_.push_back(record);
        pending_updates_.push_back(record);

        // Check if we should flush
        if (should_flush()) {
            // Move data out for flushing (doesn't block subsequent messages)
            records_to_flush = std::move(ticker_history_);
            ticker_history_.clear();
            ticker_history_.reserve(1000);  // Pre-allocate to reduce reallocations
        }
    }

    // Flush OUTSIDE lock to avoid blocking WebSocket message handler
    if (!records_to_flush.empty()) {
        flush_records_to_csv(records_to_flush);

        // Update flush time and count
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            last_flush_time_ = std::chrono::steady_clock::now();
            flush_count_++;
        }
    }

    // Call user callback (outside data lock)
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (update_callback_) {
            update_callback_(record);
        }
    }
}

template<typename JsonParser>
bool KrakenWebSocketClientBase<JsonParser>::should_flush() const {
    // Must be called with data_mutex_ held!

    // Don't flush if no output file configured
    if (output_filename_.empty()) {
        return false;
    }

    // Don't flush if no data
    if (ticker_history_.empty()) {
        return false;
    }

    // Check time-based condition (if enabled, interval > 0)
    bool time_exceeded = false;
    if (flush_interval_.count() > 0) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_flush_time_);
        time_exceeded = (elapsed >= flush_interval_);
    }

    // Check memory-based condition (if enabled, threshold > 0)
    bool memory_exceeded = false;
    if (memory_threshold_bytes_ > 0) {
        size_t memory_bytes = ticker_history_.size() * sizeof(TickerRecord);
        memory_exceeded = (memory_bytes >= memory_threshold_bytes_);
    }

    // Flush if EITHER condition is met
    return time_exceeded || memory_exceeded;
}

template<typename JsonParser>
std::string KrakenWebSocketClientBase<JsonParser>::generate_segment_key() const {
    // Must be called with data_mutex_ held!

    if (segment_mode_ == SegmentMode::NONE) {
        return "";
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&time_t);

    std::ostringstream oss;

    // Date part: YYYYMMDD
    oss << std::setfill('0')
        << std::setw(4) << (tm.tm_year + 1900)
        << std::setw(2) << (tm.tm_mon + 1)
        << std::setw(2) << tm.tm_mday;

    // Hour part for hourly mode: _HH
    if (segment_mode_ == SegmentMode::HOURLY) {
        oss << "_" << std::setw(2) << tm.tm_hour;
    }

    return oss.str();
}

template<typename JsonParser>
std::string KrakenWebSocketClientBase<JsonParser>::generate_segment_filename() const {
    // Must be called with data_mutex_ held!

    if (segment_mode_ == SegmentMode::NONE || base_output_filename_.empty()) {
        return base_output_filename_;
    }

    std::string segment_key = generate_segment_key();
    if (segment_key.empty()) {
        return base_output_filename_;
    }

    // Find the last dot in filename to insert before extension
    size_t dot_pos = base_output_filename_.find_last_of('.');

    if (dot_pos != std::string::npos) {
        // Has extension: insert before it
        // "output.csv" -> "output.20251112_10.csv"
        return base_output_filename_.substr(0, dot_pos) + "." +
               segment_key +
               base_output_filename_.substr(dot_pos);
    } else {
        // No extension: append to end
        // "output" -> "output.20251112_10"
        return base_output_filename_ + "." + segment_key;
    }
}

template<typename JsonParser>
bool KrakenWebSocketClientBase<JsonParser>::should_start_new_segment() const {
    // Must be called with data_mutex_ held!

    if (segment_mode_ == SegmentMode::NONE) {
        return false;
    }

    std::string new_key = generate_segment_key();

    // First segment or segment changed
    return current_segment_key_.empty() || (current_segment_key_ != new_key);
}

template<typename JsonParser>
void KrakenWebSocketClientBase<JsonParser>::flush_records_to_csv(
    const std::vector<TickerRecord>& records) {

    if (records.empty()) {
        return;
    }

    // Check if we need to start a new segment
    if (should_start_new_segment()) {
        current_segment_key_ = generate_segment_key();
        current_segment_filename_ = generate_segment_filename();
        csv_header_written_ = false;  // New file needs header
        segment_count_++;

        std::cout << "[SEGMENT] Starting new file: " << current_segment_filename_ << std::endl;
    }

    // Determine which filename to use
    std::string target_filename = (segment_mode_ == SegmentMode::NONE) ?
                                  output_filename_ : current_segment_filename_;

    // Determine open mode
    std::ios_base::openmode mode = std::ios::out;
    if (csv_header_written_) {
        mode |= std::ios::app;  // Append mode
    }

    std::ofstream file(target_filename, mode);
    if (!file.is_open()) {
        std::cerr << "[Error] Could not open file " << target_filename << std::endl;
        return;
    }

    // Write header only on first write
    if (!csv_header_written_) {
        file << "timestamp,pair,type,bid,bid_qty,ask,ask_qty,last,volume,vwap,low,high,change,change_pct\n";
        csv_header_written_ = true;
    }

    // Write data
    for (const auto& record : records) {
        file << record.timestamp << ","
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

    file.close();

    // Log flush (quiet output, only show on first few flushes)
    static size_t log_count = 0;
    if (log_count < 3) {
        std::cout << "[FLUSH] Wrote " << records.size() << " records to " << target_filename << std::endl;
        log_count++;
    }
}

} // namespace kraken

#endif // KRAKEN_WEBSOCKET_CLIENT_BASE_HPP
