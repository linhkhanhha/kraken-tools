/**
 * Order Book State Rebuilder
 *
 * Maintains order book state by applying snapshots and updates.
 * Used by process_orderbook_snapshots tool to rebuild state from raw .jsonl data.
 */

#ifndef ORDERBOOK_STATE_HPP
#define ORDERBOOK_STATE_HPP

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include "orderbook_common.hpp"

namespace kraken {

/**
 * Order Book State - maintains current state for one symbol
 */
class OrderBookState {
public:
    OrderBookState(const std::string& symbol);

    /**
     * Apply an order book record (snapshot or update)
     */
    void apply(const OrderBookRecord& record);

    /**
     * Reset state (clear all levels)
     */
    void reset();

    /**
     * Get current best bid price and quantity
     */
    bool get_best_bid(double& price, double& quantity) const;

    /**
     * Get current best ask price and quantity
     */
    bool get_best_ask(double& price, double& quantity) const;

    /**
     * Get top N bid levels (sorted high to low)
     */
    std::vector<PriceLevel> get_top_bids(int n) const;

    /**
     * Get top N ask levels (sorted low to high)
     */
    std::vector<PriceLevel> get_top_asks(int n) const;

    /**
     * Get all bids within X basis points of reference price
     */
    double get_bid_volume_within_bps(double reference_price, double bps) const;

    /**
     * Get all asks within X basis points of reference price
     */
    double get_ask_volume_within_bps(double reference_price, double bps) const;

    /**
     * Get total volume in top N levels
     */
    double get_bid_volume_top_n(int n) const;
    double get_ask_volume_top_n(int n) const;

    /**
     * Validate current state against checksum
     */
    bool validate_checksum(uint32_t expected_checksum) const;

    /**
     * Get symbol
     */
    const std::string& symbol() const { return symbol_; }

    /**
     * Check if state is initialized
     */
    bool is_initialized() const { return initialized_; }

private:
    std::string symbol_;
    bool initialized_;

    // Order book state: price -> quantity
    // Using std::map for automatic sorting
    std::map<double, double, std::greater<double>> bids_;  // Descending (high to low)
    std::map<double, double> asks_;                        // Ascending (low to high)

    /**
     * Apply price levels from record
     */
    void apply_levels(const std::vector<PriceLevel>& levels,
                     std::map<double, double, std::greater<double>>& bids_map);
    void apply_levels(const std::vector<PriceLevel>& levels,
                     std::map<double, double>& asks_map);
};

/**
 * Snapshot Metrics - calculated from order book state at a point in time
 */
struct SnapshotMetrics {
    std::string timestamp;
    std::string symbol;

    // Best bid/ask
    double best_bid;
    double best_bid_qty;
    double best_ask;
    double best_ask_qty;

    // Derived metrics
    double spread;           // best_ask - best_bid
    double spread_bps;       // spread in basis points relative to mid
    double mid_price;        // (best_bid + best_ask) / 2

    // Volume metrics
    double bid_volume_top10;
    double ask_volume_top10;

    // Imbalance: (bid_vol - ask_vol) / (bid_vol + ask_vol)
    // Range: -1.0 (all asks) to +1.0 (all bids)
    double imbalance;

    // Depth within X basis points of mid
    double depth_10_bps;
    double depth_25_bps;
    double depth_50_bps;

    SnapshotMetrics()
        : best_bid(0.0), best_bid_qty(0.0), best_ask(0.0), best_ask_qty(0.0),
          spread(0.0), spread_bps(0.0), mid_price(0.0),
          bid_volume_top10(0.0), ask_volume_top10(0.0),
          imbalance(0.0),
          depth_10_bps(0.0), depth_25_bps(0.0), depth_50_bps(0.0)
    {}
};

/**
 * Metrics Calculator - calculates metrics from order book state
 */
class MetricsCalculator {
public:
    /**
     * Calculate all metrics from current order book state
     */
    static SnapshotMetrics calculate(const OrderBookState& state, const std::string& timestamp);

private:
    static double calculate_basis_points(double value, double reference);
};

} // namespace kraken

#endif // ORDERBOOK_STATE_HPP
