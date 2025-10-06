/**
 * Order Book Common Data Structures and Utilities - Implementation
 */

#include "orderbook_common.hpp"
#include <algorithm>
#include <cstring>
#include <map>

namespace kraken {

// ============================================================================
// ChecksumValidator Implementation
// ============================================================================

uint32_t ChecksumValidator::crc32_table[256];
bool ChecksumValidator::table_initialized = false;

void ChecksumValidator::init_crc32_table() {
    if (table_initialized) return;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    table_initialized = true;
}

uint32_t ChecksumValidator::crc32_update(uint32_t crc, const char* data, size_t len) {
    init_crc32_table();

    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc;
}

std::string ChecksumValidator::format_for_checksum(
    const std::vector<PriceLevel>& bids,
    const std::vector<PriceLevel>& asks
) {
    std::ostringstream oss;

    // Take top 10 bids and asks (or less if not available)
    size_t num_levels = std::min(size_t(10), std::min(bids.size(), asks.size()));

    for (size_t i = 0; i < num_levels; i++) {
        // Format: price,quantity for asks
        oss << std::fixed << std::setprecision(10) << asks[i].price;
        oss << std::fixed << std::setprecision(8) << asks[i].quantity;
    }

    for (size_t i = 0; i < num_levels; i++) {
        // Format: price,quantity for bids
        oss << std::fixed << std::setprecision(10) << bids[i].price;
        oss << std::fixed << std::setprecision(8) << bids[i].quantity;
    }

    return oss.str();
}

uint32_t ChecksumValidator::calculate_crc32(
    const std::vector<PriceLevel>& bids,
    const std::vector<PriceLevel>& asks
) {
    std::string data = format_for_checksum(bids, asks);
    uint32_t crc = 0xFFFFFFFF;
    crc = crc32_update(crc, data.c_str(), data.length());
    return crc ^ 0xFFFFFFFF;
}

bool ChecksumValidator::validate(const OrderBookRecord& record) {
    if (record.bids.empty() || record.asks.empty()) {
        return true; // Can't validate empty book
    }

    uint32_t calculated = calculate_crc32(record.bids, record.asks);
    return calculated == record.checksum;
}

// ============================================================================
// OrderBookDisplay Implementation
// ============================================================================

std::string OrderBookDisplay::format_price(double price, int width) {
    std::ostringstream oss;
    oss << "$" << std::fixed << std::setprecision(2) << std::setw(width - 1) << price;
    return oss.str();
}

std::string OrderBookDisplay::format_quantity(double qty, int width) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << std::setw(width) << qty;
    return oss.str();
}

void OrderBookDisplay::print_separator(int width) {
    std::cout << std::string(width, '-') << std::endl;
}

void OrderBookDisplay::show_minimal(const std::map<std::string, OrderBookStats>& stats) {
    std::cout << "[STATUS] ";
    bool first = true;
    for (const auto& pair : stats) {
        if (!first) std::cout << " | ";
        std::cout << pair.first << ": "
                  << pair.second.snapshot_count << " snapshots, "
                  << pair.second.update_count << " updates";
        first = false;
    }
    std::cout << std::endl;
}

void OrderBookDisplay::show_update_details(const OrderBookRecord& record, const std::string& prefix) {
    std::cout << prefix << " " << record.symbol << ": ";

    if (record.type == "snapshot") {
        std::cout << record.bids.size() << " bids, "
                  << record.asks.size() << " asks"
                  << std::endl;
    } else {
        // Count non-zero quantities (actual updates)
        int bid_changes = 0, ask_changes = 0;
        for (const auto& bid : record.bids) {
            if (bid.quantity > 0) bid_changes++;
        }
        for (const auto& ask : record.asks) {
            if (ask.quantity > 0) ask_changes++;
        }

        std::cout << bid_changes << " bid" << (bid_changes != 1 ? "s" : "") << " changed, "
                  << ask_changes << " ask" << (ask_changes != 1 ? "s" : "") << " changed"
                  << std::endl;
    }
}

void OrderBookDisplay::show_top_of_book(const OrderBookRecord& record) {
    if (record.bids.empty() || record.asks.empty()) {
        return;
    }

    double best_bid = record.bids[0].price;
    double best_bid_qty = record.bids[0].quantity;
    double best_ask = record.asks[0].price;
    double best_ask_qty = record.asks[0].quantity;
    double spread = best_ask - best_bid;

    std::cout << "[" << record.symbol << "] "
              << "Bid: " << format_price(best_bid, 12) << " (" << best_bid_qty << ") | "
              << "Ask: " << format_price(best_ask, 12) << " (" << best_ask_qty << ") | "
              << "Spread: " << format_price(spread, 8)
              << std::endl;
}

void OrderBookDisplay::show_full_book(const OrderBookRecord& record, int max_depth) {
    if (record.bids.empty() || record.asks.empty()) {
        std::cout << "[" << record.symbol << "] Order book empty" << std::endl;
        return;
    }

    int depth = std::min(max_depth, static_cast<int>(std::min(record.bids.size(), record.asks.size())));

    std::cout << "\n+--- " << record.symbol << " Order Book (Depth: " << depth << ") ---+" << std::endl;
    std::cout << "| Bids                  | Asks                  |" << std::endl;
    std::cout << "+-----------------------+-----------------------+" << std::endl;

    for (int i = 0; i < depth; i++) {
        std::cout << "| " << format_price(record.bids[i].price, 10)
                  << " [" << format_quantity(record.bids[i].quantity, 6) << "] | "
                  << "[" << format_quantity(record.asks[i].quantity, 6) << "] "
                  << format_price(record.asks[i].price, 10)
                  << " |" << std::endl;
    }

    std::cout << "+-----------------------------------------------+\n" << std::endl;
}

void OrderBookDisplay::update_stats(OrderBookStats& stats, const OrderBookRecord& record) {
    stats.total_messages++;

    if (record.type == "snapshot") {
        stats.snapshot_count++;
    } else {
        stats.update_count++;
    }

    // Update top-of-book if available
    if (!record.bids.empty() && !record.asks.empty()) {
        stats.best_bid = record.bids[0].price;
        stats.best_bid_qty = record.bids[0].quantity;
        stats.best_ask = record.asks[0].price;
        stats.best_ask_qty = record.asks[0].quantity;
        stats.spread = stats.best_ask - stats.best_bid;
    }
}

} // namespace kraken
