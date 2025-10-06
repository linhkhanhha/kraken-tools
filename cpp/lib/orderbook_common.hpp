/**
 * Order Book Common Data Structures and Utilities
 *
 * Shared data structures for Level 2 order book data from Kraken WebSocket v2
 */

#ifndef ORDERBOOK_COMMON_HPP
#define ORDERBOOK_COMMON_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <map>

namespace kraken {

/**
 * Single price level in the order book
 */
struct PriceLevel {
    double price;
    double quantity;

    PriceLevel() : price(0.0), quantity(0.0) {}
    PriceLevel(double p, double q) : price(p), quantity(q) {}
};

/**
 * Order book record structure - matches Kraken WebSocket v2 book data
 */
struct OrderBookRecord {
    std::string timestamp;
    std::string symbol;
    std::string type;                    // "snapshot" or "update"
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    uint32_t checksum;

    OrderBookRecord() : checksum(0) {}
};

/**
 * Statistics for order book updates (per symbol)
 */
struct OrderBookStats {
    int snapshot_count;
    int update_count;
    int total_messages;

    // For top-of-book tracking
    double best_bid;
    double best_bid_qty;
    double best_ask;
    double best_ask_qty;
    double spread;

    OrderBookStats()
        : snapshot_count(0)
        , update_count(0)
        , total_messages(0)
        , best_bid(0.0)
        , best_bid_qty(0.0)
        , best_ask(0.0)
        , best_ask_qty(0.0)
        , spread(0.0)
    {}
};

/**
 * CRC32 Checksum Validator
 * Validates Kraken's order book checksums
 */
class ChecksumValidator {
public:
    /**
     * Calculate CRC32 checksum for order book data
     * Following Kraken's specification for top 10 bids/asks
     */
    static uint32_t calculate_crc32(const std::vector<PriceLevel>& bids,
                                     const std::vector<PriceLevel>& asks);

    /**
     * Validate checksum against order book record
     */
    static bool validate(const OrderBookRecord& record);

    /**
     * Format price levels for checksum calculation
     */
    static std::string format_for_checksum(const std::vector<PriceLevel>& bids,
                                            const std::vector<PriceLevel>& asks);

private:
    // CRC32 lookup table
    static uint32_t crc32_table[256];
    static bool table_initialized;

    static void init_crc32_table();
    static uint32_t crc32_update(uint32_t crc, const char* data, size_t len);
};

/**
 * Order Book Display Utilities
 */
class OrderBookDisplay {
public:
    /**
     * Show minimal status (default mode)
     * Just counts per symbol
     */
    static void show_minimal(const std::map<std::string, OrderBookStats>& stats);

    /**
     * Show update details
     * What changed in the update
     */
    static void show_update_details(const OrderBookRecord& record, const std::string& prefix = "[UPDATE]");

    /**
     * Show top-of-book (best bid/ask)
     */
    static void show_top_of_book(const OrderBookRecord& record);

    /**
     * Show full order book
     * Terminal-based display (single pair only)
     */
    static void show_full_book(const OrderBookRecord& record, int max_depth = 10);

    /**
     * Update statistics from record
     */
    static void update_stats(OrderBookStats& stats, const OrderBookRecord& record);

private:
    static std::string format_price(double price, int width = 10);
    static std::string format_quantity(double qty, int width = 8);
    static void print_separator(int width);
};

} // namespace kraken

#endif // ORDERBOOK_COMMON_HPP
