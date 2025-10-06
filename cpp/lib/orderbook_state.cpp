/**
 * Order Book State Rebuilder - Implementation
 */

#include "orderbook_state.hpp"
#include <algorithm>
#include <cmath>

namespace kraken {

// ============================================================================
// OrderBookState Implementation
// ============================================================================

OrderBookState::OrderBookState(const std::string& symbol)
    : symbol_(symbol), initialized_(false) {
}

void OrderBookState::apply(const OrderBookRecord& record) {
    if (record.type == "snapshot") {
        // Reset and initialize from snapshot
        reset();

        // Apply all bid levels
        for (const auto& level : record.bids) {
            if (level.quantity > 0.0) {
                bids_[level.price] = level.quantity;
            }
        }

        // Apply all ask levels
        for (const auto& level : record.asks) {
            if (level.quantity > 0.0) {
                asks_[level.price] = level.quantity;
            }
        }

        initialized_ = true;

    } else if (record.type == "update") {
        // Apply incremental updates

        // Update bids
        for (const auto& level : record.bids) {
            if (level.quantity > 0.0) {
                // Add or update level
                bids_[level.price] = level.quantity;
            } else {
                // Remove level (quantity = 0)
                bids_.erase(level.price);
            }
        }

        // Update asks
        for (const auto& level : record.asks) {
            if (level.quantity > 0.0) {
                // Add or update level
                asks_[level.price] = level.quantity;
            } else {
                // Remove level (quantity = 0)
                asks_.erase(level.price);
            }
        }
    }
}

void OrderBookState::reset() {
    bids_.clear();
    asks_.clear();
    initialized_ = false;
}

bool OrderBookState::get_best_bid(double& price, double& quantity) const {
    if (bids_.empty()) {
        return false;
    }

    // std::map with std::greater is sorted descending, so first element is highest
    auto it = bids_.begin();
    price = it->first;
    quantity = it->second;
    return true;
}

bool OrderBookState::get_best_ask(double& price, double& quantity) const {
    if (asks_.empty()) {
        return false;
    }

    // std::map is sorted ascending, so first element is lowest
    auto it = asks_.begin();
    price = it->first;
    quantity = it->second;
    return true;
}

std::vector<PriceLevel> OrderBookState::get_top_bids(int n) const {
    std::vector<PriceLevel> result;
    result.reserve(std::min(n, static_cast<int>(bids_.size())));

    int count = 0;
    for (const auto& pair : bids_) {
        if (count >= n) break;
        result.emplace_back(pair.first, pair.second);
        count++;
    }

    return result;
}

std::vector<PriceLevel> OrderBookState::get_top_asks(int n) const {
    std::vector<PriceLevel> result;
    result.reserve(std::min(n, static_cast<int>(asks_.size())));

    int count = 0;
    for (const auto& pair : asks_) {
        if (count >= n) break;
        result.emplace_back(pair.first, pair.second);
        count++;
    }

    return result;
}

double OrderBookState::get_bid_volume_within_bps(double reference_price, double bps) const {
    // Calculate price threshold
    // For bids, we want prices >= (reference * (1 - bps/10000))
    double threshold = reference_price * (1.0 - bps / 10000.0);

    double total_volume = 0.0;
    for (const auto& pair : bids_) {
        if (pair.first >= threshold) {
            total_volume += pair.second;
        } else {
            break;  // Sorted descending, can stop early
        }
    }

    return total_volume;
}

double OrderBookState::get_ask_volume_within_bps(double reference_price, double bps) const {
    // Calculate price threshold
    // For asks, we want prices <= (reference * (1 + bps/10000))
    double threshold = reference_price * (1.0 + bps / 10000.0);

    double total_volume = 0.0;
    for (const auto& pair : asks_) {
        if (pair.first <= threshold) {
            total_volume += pair.second;
        } else {
            break;  // Sorted ascending, can stop early
        }
    }

    return total_volume;
}

double OrderBookState::get_bid_volume_top_n(int n) const {
    double total_volume = 0.0;
    int count = 0;

    for (const auto& pair : bids_) {
        if (count >= n) break;
        total_volume += pair.second;
        count++;
    }

    return total_volume;
}

double OrderBookState::get_ask_volume_top_n(int n) const {
    double total_volume = 0.0;
    int count = 0;

    for (const auto& pair : asks_) {
        if (count >= n) break;
        total_volume += pair.second;
        count++;
    }

    return total_volume;
}

bool OrderBookState::validate_checksum(uint32_t expected_checksum) const {
    // Build vectors for checksum calculation
    std::vector<PriceLevel> top_bids = get_top_bids(10);
    std::vector<PriceLevel> top_asks = get_top_asks(10);

    uint32_t calculated = ChecksumValidator::calculate_crc32(top_bids, top_asks);
    return calculated == expected_checksum;
}

// ============================================================================
// MetricsCalculator Implementation
// ============================================================================

SnapshotMetrics MetricsCalculator::calculate(const OrderBookState& state, const std::string& timestamp) {
    SnapshotMetrics metrics;
    metrics.timestamp = timestamp;
    metrics.symbol = state.symbol();

    // Get best bid and ask
    if (!state.get_best_bid(metrics.best_bid, metrics.best_bid_qty)) {
        // No bids available - return empty metrics
        return metrics;
    }

    if (!state.get_best_ask(metrics.best_ask, metrics.best_ask_qty)) {
        // No asks available - return empty metrics
        return metrics;
    }

    // Calculate derived metrics
    metrics.spread = metrics.best_ask - metrics.best_bid;
    metrics.mid_price = (metrics.best_bid + metrics.best_ask) / 2.0;
    metrics.spread_bps = calculate_basis_points(metrics.spread, metrics.mid_price);

    // Calculate volume metrics
    metrics.bid_volume_top10 = state.get_bid_volume_top_n(10);
    metrics.ask_volume_top10 = state.get_ask_volume_top_n(10);

    // Calculate imbalance
    double total_volume = metrics.bid_volume_top10 + metrics.ask_volume_top10;
    if (total_volume > 0.0) {
        metrics.imbalance = (metrics.bid_volume_top10 - metrics.ask_volume_top10) / total_volume;
    }

    // Calculate depth at various basis points
    double bid_vol_10 = state.get_bid_volume_within_bps(metrics.mid_price, 10.0);
    double ask_vol_10 = state.get_ask_volume_within_bps(metrics.mid_price, 10.0);
    metrics.depth_10_bps = bid_vol_10 + ask_vol_10;

    double bid_vol_25 = state.get_bid_volume_within_bps(metrics.mid_price, 25.0);
    double ask_vol_25 = state.get_ask_volume_within_bps(metrics.mid_price, 25.0);
    metrics.depth_25_bps = bid_vol_25 + ask_vol_25;

    double bid_vol_50 = state.get_bid_volume_within_bps(metrics.mid_price, 50.0);
    double ask_vol_50 = state.get_ask_volume_within_bps(metrics.mid_price, 50.0);
    metrics.depth_50_bps = bid_vol_50 + ask_vol_50;

    return metrics;
}

double MetricsCalculator::calculate_basis_points(double value, double reference) {
    if (reference == 0.0) {
        return 0.0;
    }
    return (value / reference) * 10000.0;
}

} // namespace kraken
