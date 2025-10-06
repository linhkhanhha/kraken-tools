/**
 * Level 3 Order Book State Rebuilder
 *
 * Maintains individual order-level state with dual indexing:
 * 1. By order ID - for fast updates/deletes (O(log n) lookup)
 * 2. By price level - for fast metrics calculation (O(1) best price access)
 */

#ifndef LEVEL3_STATE_HPP
#define LEVEL3_STATE_HPP

#include <string>
#include <map>
#include <vector>
#include <memory>
#include "level3_common.hpp"

namespace kraken {

/**
 * Internal order representation
 */
struct Order {
    std::string order_id;
    double limit_price;
    double order_qty;
    std::string timestamp;

    Order(const std::string& id, double price, double qty, const std::string& ts)
        : order_id(id), limit_price(price), order_qty(qty), timestamp(ts) {}
};

/**
 * Metrics snapshot from Level 3 order book state
 */
struct Level3SnapshotMetrics {
    std::string timestamp;
    std::string symbol;

    // Top of book
    double best_bid;
    double best_bid_qty;
    double best_ask;
    double best_ask_qty;
    double spread;
    double spread_bps;
    double mid_price;

    // Volume metrics (aggregated from orders)
    double bid_volume_top10;
    double ask_volume_top10;
    double imbalance;

    // Depth metrics
    double depth_10_bps;
    double depth_25_bps;
    double depth_50_bps;

    // Order counts (Level 3 specific)
    int bid_order_count;
    int ask_order_count;
    int bid_orders_at_best;
    int ask_orders_at_best;

    // Average order sizes (Level 3 specific)
    double avg_bid_order_size;
    double avg_ask_order_size;

    // Flow metrics (Level 3 specific)
    int add_events;
    int modify_events;
    int delete_events;
    double order_arrival_rate;
    double order_cancel_rate;

    Level3SnapshotMetrics()
        : best_bid(0), best_bid_qty(0), best_ask(0), best_ask_qty(0),
          spread(0), spread_bps(0), mid_price(0),
          bid_volume_top10(0), ask_volume_top10(0), imbalance(0),
          depth_10_bps(0), depth_25_bps(0), depth_50_bps(0),
          bid_order_count(0), ask_order_count(0),
          bid_orders_at_best(0), ask_orders_at_best(0),
          avg_bid_order_size(0), avg_ask_order_size(0),
          add_events(0), modify_events(0), delete_events(0),
          order_arrival_rate(0), order_cancel_rate(0) {}
};

/**
 * Level 3 Order Book State
 *
 * Dual indexing for efficient operations:
 * - orders_by_id_: Fast O(log n) lookup for updates/deletes
 * - bids/asks_by_price_: Fast O(1) iteration for metrics
 */
class Level3OrderBookState {
public:
    /**
     * Constructor
     */
    Level3OrderBookState(const std::string& symbol);

    /**
     * Destructor - clean up order pointers
     */
    ~Level3OrderBookState();

    // Disable copy
    Level3OrderBookState(const Level3OrderBookState&) = delete;
    Level3OrderBookState& operator=(const Level3OrderBookState&) = delete;

    /**
     * Apply a snapshot (full order book)
     * Clears existing state and initializes from snapshot
     */
    void apply_snapshot(const Level3Record& record);

    /**
     * Apply an update (add/modify/delete events)
     * Processes individual order events
     */
    void apply_update(const Level3Record& record);

    /**
     * Get best bid/ask
     */
    bool get_best_bid(double& price, double& total_qty) const;
    bool get_best_ask(double& price, double& total_qty) const;

    /**
     * Get order counts
     */
    int get_total_bid_orders() const;
    int get_total_ask_orders() const;
    int get_bid_orders_at_price(double price) const;
    int get_ask_orders_at_price(double price) const;

    /**
     * Get aggregated volume at price level
     */
    double get_bid_volume_at_price(double price) const;
    double get_ask_volume_at_price(double price) const;

    /**
     * Get volume within BPS of reference price
     */
    double get_bid_volume_within_bps(double reference_price, double bps) const;
    double get_ask_volume_within_bps(double reference_price, double bps) const;

    /**
     * Get average order sizes
     */
    double get_avg_bid_order_size() const;
    double get_avg_ask_order_size() const;

    /**
     * Get event counts (since last reset)
     */
    int get_add_count() const { return add_count_; }
    int get_modify_count() const { return modify_count_; }
    int get_delete_count() const { return delete_count_; }

    /**
     * Reset event counters (for interval-based metrics)
     */
    void reset_event_counters();

    /**
     * Get symbol
     */
    std::string get_symbol() const { return symbol_; }

    /**
     * Calculate comprehensive metrics
     */
    Level3SnapshotMetrics calculate_metrics(const std::string& timestamp) const;

private:
    std::string symbol_;

    // Dual indexing: By order ID (for fast lookup)
    std::map<std::string, std::shared_ptr<Order>> orders_by_id_;

    // Dual indexing: By price (for fast iteration)
    // Using vectors to hold multiple orders at same price
    // Bids: descending order (highest first)
    std::map<double, std::vector<std::shared_ptr<Order>>, std::greater<double>> bids_by_price_;
    // Asks: ascending order (lowest first)
    std::map<double, std::vector<std::shared_ptr<Order>>> asks_by_price_;

    // Event counters
    int add_count_;
    int modify_count_;
    int delete_count_;

    // Helper methods
    void clear_all_orders();
    void add_order(const Level3Order& order, bool is_bid);
    void modify_order(const std::string& order_id, double new_price, double new_qty);
    void delete_order(const std::string& order_id);
    void remove_from_price_index(std::shared_ptr<Order> order, bool is_bid);
    void add_to_price_index(std::shared_ptr<Order> order, bool is_bid);
};

} // namespace kraken

#endif // LEVEL3_STATE_HPP
