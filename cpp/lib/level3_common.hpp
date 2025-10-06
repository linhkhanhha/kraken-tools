/**
 * Level 3 Order Book Common Data Structures
 *
 * Individual order-level data from Kraken WebSocket v2 Level 3 channel
 */

#ifndef LEVEL3_COMMON_HPP
#define LEVEL3_COMMON_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <map>

namespace kraken {

/**
 * Single order in Level 3 order book
 */
struct Level3Order {
    std::string order_id;
    double limit_price;
    double order_qty;
    std::string timestamp;  // RFC3339 format

    // For updates
    std::string event;  // "add", "modify", "delete" (empty for snapshots)

    Level3Order()
        : limit_price(0.0), order_qty(0.0)
    {}

    Level3Order(const std::string& id, double price, double qty, const std::string& ts)
        : order_id(id), limit_price(price), order_qty(qty), timestamp(ts)
    {}
};

/**
 * Level 3 order book record - matches Kraken WebSocket v2 level3 channel
 */
struct Level3Record {
    std::string timestamp;
    std::string symbol;
    std::string type;  // "snapshot" or "update"
    std::vector<Level3Order> bids;
    std::vector<Level3Order> asks;
    uint32_t checksum;

    Level3Record() : checksum(0) {}
};

/**
 * Statistics for Level 3 order book (per symbol)
 */
struct Level3Stats {
    int snapshot_count;
    int update_count;
    int total_messages;

    // Order event counts
    int add_events;
    int modify_events;
    int delete_events;

    // Current state
    int bid_order_count;
    int ask_order_count;
    double best_bid;
    double best_ask;
    double spread;

    Level3Stats()
        : snapshot_count(0)
        , update_count(0)
        , total_messages(0)
        , add_events(0)
        , modify_events(0)
        , delete_events(0)
        , bid_order_count(0)
        , ask_order_count(0)
        , best_bid(0.0)
        , best_ask(0.0)
        , spread(0.0)
    {}
};

/**
 * Level 3 Display Utilities
 */
class Level3Display {
public:
    /**
     * Show minimal status (default mode)
     * Counters per symbol
     */
    static void show_minimal(const std::map<std::string, Level3Stats>& stats);

    /**
     * Show event counts
     * Add/modify/delete statistics
     */
    static void show_event_counts(const std::map<std::string, Level3Stats>& stats);

    /**
     * Show top-of-book with order details
     */
    static void show_top_of_book(const Level3Record& record);

    /**
     * Show order event (for --show-orders mode)
     */
    static void show_order_event(const Level3Order& order, const std::string& symbol, bool is_bid);

    /**
     * Update statistics from record
     */
    static void update_stats(Level3Stats& stats, const Level3Record& record);

private:
    static std::string format_price(double price, int width = 12);
    static std::string format_quantity(double qty, int width = 10);
};

} // namespace kraken

#endif // LEVEL3_COMMON_HPP
