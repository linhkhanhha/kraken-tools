/**
 * Level 3 Order Book Common - Implementation
 */

#include "level3_common.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace kraken {

// ============================================================================
// Level3Display Implementation
// ============================================================================

std::string Level3Display::format_price(double price, int width) {
    std::ostringstream oss;
    oss << "$" << std::fixed << std::setprecision(2) << std::setw(width - 1) << price;
    return oss.str();
}

std::string Level3Display::format_quantity(double qty, int width) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << std::setw(width) << qty;
    return oss.str();
}

void Level3Display::show_minimal(const std::map<std::string, Level3Stats>& stats) {
    std::cout << "[STATUS] ";
    bool first = true;
    for (const auto& pair : stats) {
        if (!first) std::cout << " | ";
        std::cout << pair.first << ": "
                  << pair.second.snapshot_count << " snapshots, "
                  << pair.second.update_count << " updates, "
                  << pair.second.bid_order_count << " bids, "
                  << pair.second.ask_order_count << " asks";
        first = false;
    }
    std::cout << std::endl;
}

void Level3Display::show_event_counts(const std::map<std::string, Level3Stats>& stats) {
    std::cout << "[EVENTS] ";
    bool first = true;
    for (const auto& pair : stats) {
        if (!first) std::cout << " | ";
        std::cout << pair.first << ": "
                  << "add:" << pair.second.add_events << " "
                  << "modify:" << pair.second.modify_events << " "
                  << "delete:" << pair.second.delete_events;
        first = false;
    }
    std::cout << std::endl;
}

void Level3Display::show_top_of_book(const Level3Record& record) {
    if (record.bids.empty() || record.asks.empty()) {
        return;
    }

    const Level3Order& best_bid = record.bids[0];
    const Level3Order& best_ask = record.asks[0];
    double spread = best_ask.limit_price - best_bid.limit_price;

    std::cout << "[" << record.symbol << "] "
              << "Bid: " << format_price(best_bid.limit_price, 12)
              << " (" << best_bid.order_qty << ") [" << best_bid.order_id << "] | "
              << "Ask: " << format_price(best_ask.limit_price, 12)
              << " (" << best_ask.order_qty << ") [" << best_ask.order_id << "] | "
              << "Spread: " << format_price(spread, 8)
              << std::endl;
}

void Level3Display::show_order_event(const Level3Order& order, const std::string& symbol, bool is_bid) {
    std::string side = is_bid ? "BID" : "ASK";
    std::string event_upper = order.event;
    // Convert to uppercase for display
    for (char& c : event_upper) {
        c = std::toupper(c);
    }

    std::cout << "[ORDER] " << symbol << " " << side << " "
              << event_upper << " "
              << order.order_id << " @ "
              << format_price(order.limit_price, 12) << " x "
              << order.order_qty
              << std::endl;
}

void Level3Display::update_stats(Level3Stats& stats, const Level3Record& record) {
    stats.total_messages++;

    if (record.type == "snapshot") {
        stats.snapshot_count++;
        stats.bid_order_count = record.bids.size();
        stats.ask_order_count = record.asks.size();
    } else if (record.type == "update") {
        stats.update_count++;

        // Count events
        for (const auto& bid : record.bids) {
            if (bid.event == "add") {
                stats.add_events++;
                stats.bid_order_count++;
            } else if (bid.event == "modify") {
                stats.modify_events++;
            } else if (bid.event == "delete") {
                stats.delete_events++;
                stats.bid_order_count--;
            }
        }

        for (const auto& ask : record.asks) {
            if (ask.event == "add") {
                stats.add_events++;
                stats.ask_order_count++;
            } else if (ask.event == "modify") {
                stats.modify_events++;
            } else if (ask.event == "delete") {
                stats.delete_events++;
                stats.ask_order_count--;
            }
        }
    }

    // Update best bid/ask if available
    if (!record.bids.empty() && !record.asks.empty()) {
        stats.best_bid = record.bids[0].limit_price;
        stats.best_ask = record.asks[0].limit_price;
        stats.spread = stats.best_ask - stats.best_bid;
    }
}

} // namespace kraken
