/**
 * Level 3 Order Book State Rebuilder - Implementation
 */

#include "level3_state.hpp"
#include <algorithm>
#include <cmath>

namespace kraken {

// ============================================================================
// Level3OrderBookState Implementation
// ============================================================================

Level3OrderBookState::Level3OrderBookState(const std::string& symbol)
    : symbol_(symbol), add_count_(0), modify_count_(0), delete_count_(0) {
}

Level3OrderBookState::~Level3OrderBookState() {
    clear_all_orders();
}

void Level3OrderBookState::clear_all_orders() {
    orders_by_id_.clear();
    bids_by_price_.clear();
    asks_by_price_.clear();
}

void Level3OrderBookState::apply_snapshot(const Level3Record& record) {
    // Clear existing state
    clear_all_orders();

    // Add all bid orders
    for (const auto& order : record.bids) {
        add_order(order, true);
    }

    // Add all ask orders
    for (const auto& order : record.asks) {
        add_order(order, false);
    }
}

void Level3OrderBookState::apply_update(const Level3Record& record) {
    // Process bid updates
    for (const auto& order : record.bids) {
        if (order.event == "add") {
            add_order(order, true);
            add_count_++;
        } else if (order.event == "modify") {
            modify_order(order.order_id, order.limit_price, order.order_qty);
            modify_count_++;
        } else if (order.event == "delete") {
            delete_order(order.order_id);
            delete_count_++;
        }
    }

    // Process ask updates
    for (const auto& order : record.asks) {
        if (order.event == "add") {
            add_order(order, false);
            add_count_++;
        } else if (order.event == "modify") {
            modify_order(order.order_id, order.limit_price, order.order_qty);
            modify_count_++;
        } else if (order.event == "delete") {
            delete_order(order.order_id);
            delete_count_++;
        }
    }
}

void Level3OrderBookState::add_order(const Level3Order& order, bool is_bid) {
    // Create new order
    auto new_order = std::make_shared<Order>(
        order.order_id,
        order.limit_price,
        order.order_qty,
        order.timestamp
    );

    // Add to ID index
    orders_by_id_[order.order_id] = new_order;

    // Add to price index
    add_to_price_index(new_order, is_bid);
}

void Level3OrderBookState::modify_order(const std::string& order_id, double new_price, double new_qty) {
    // Find order by ID
    auto it = orders_by_id_.find(order_id);
    if (it == orders_by_id_.end()) {
        // Order not found, this might be a new order treated as modify
        return;
    }

    auto order = it->second;
    bool is_bid = false;

    // Determine if bid or ask by checking which index it's in
    auto bid_it = bids_by_price_.find(order->limit_price);
    if (bid_it != bids_by_price_.end()) {
        auto& orders = bid_it->second;
        auto order_it = std::find(orders.begin(), orders.end(), order);
        if (order_it != orders.end()) {
            is_bid = true;
        }
    }

    // Remove from old price level
    remove_from_price_index(order, is_bid);

    // Update order data
    order->limit_price = new_price;
    order->order_qty = new_qty;

    // Add to new price level
    add_to_price_index(order, is_bid);
}

void Level3OrderBookState::delete_order(const std::string& order_id) {
    // Find order by ID
    auto it = orders_by_id_.find(order_id);
    if (it == orders_by_id_.end()) {
        return;
    }

    auto order = it->second;
    bool is_bid = false;

    // Determine if bid or ask
    auto bid_it = bids_by_price_.find(order->limit_price);
    if (bid_it != bids_by_price_.end()) {
        auto& orders = bid_it->second;
        auto order_it = std::find(orders.begin(), orders.end(), order);
        if (order_it != orders.end()) {
            is_bid = true;
        }
    }

    // Remove from price index
    remove_from_price_index(order, is_bid);

    // Remove from ID index
    orders_by_id_.erase(it);
}

void Level3OrderBookState::add_to_price_index(std::shared_ptr<Order> order, bool is_bid) {
    if (is_bid) {
        bids_by_price_[order->limit_price].push_back(order);
    } else {
        asks_by_price_[order->limit_price].push_back(order);
    }
}

void Level3OrderBookState::remove_from_price_index(std::shared_ptr<Order> order, bool is_bid) {
    if (is_bid) {
        auto it = bids_by_price_.find(order->limit_price);
        if (it != bids_by_price_.end()) {
            auto& orders = it->second;
            orders.erase(std::remove(orders.begin(), orders.end(), order), orders.end());

            // Remove price level if empty
            if (orders.empty()) {
                bids_by_price_.erase(it);
            }
        }
    } else {
        auto it = asks_by_price_.find(order->limit_price);
        if (it != asks_by_price_.end()) {
            auto& orders = it->second;
            orders.erase(std::remove(orders.begin(), orders.end(), order), orders.end());

            // Remove price level if empty
            if (orders.empty()) {
                asks_by_price_.erase(it);
            }
        }
    }
}

bool Level3OrderBookState::get_best_bid(double& price, double& total_qty) const {
    if (bids_by_price_.empty()) {
        return false;
    }

    auto it = bids_by_price_.begin();
    price = it->first;

    // Sum all orders at this price level
    total_qty = 0;
    for (const auto& order : it->second) {
        total_qty += order->order_qty;
    }

    return true;
}

bool Level3OrderBookState::get_best_ask(double& price, double& total_qty) const {
    if (asks_by_price_.empty()) {
        return false;
    }

    auto it = asks_by_price_.begin();
    price = it->first;

    // Sum all orders at this price level
    total_qty = 0;
    for (const auto& order : it->second) {
        total_qty += order->order_qty;
    }

    return true;
}

int Level3OrderBookState::get_total_bid_orders() const {
    int count = 0;
    for (const auto& pair : bids_by_price_) {
        count += pair.second.size();
    }
    return count;
}

int Level3OrderBookState::get_total_ask_orders() const {
    int count = 0;
    for (const auto& pair : asks_by_price_) {
        count += pair.second.size();
    }
    return count;
}

int Level3OrderBookState::get_bid_orders_at_price(double price) const {
    auto it = bids_by_price_.find(price);
    if (it == bids_by_price_.end()) {
        return 0;
    }
    return it->second.size();
}

int Level3OrderBookState::get_ask_orders_at_price(double price) const {
    auto it = asks_by_price_.find(price);
    if (it == asks_by_price_.end()) {
        return 0;
    }
    return it->second.size();
}

double Level3OrderBookState::get_bid_volume_at_price(double price) const {
    auto it = bids_by_price_.find(price);
    if (it == bids_by_price_.end()) {
        return 0.0;
    }

    double total = 0.0;
    for (const auto& order : it->second) {
        total += order->order_qty;
    }
    return total;
}

double Level3OrderBookState::get_ask_volume_at_price(double price) const {
    auto it = asks_by_price_.find(price);
    if (it == asks_by_price_.end()) {
        return 0.0;
    }

    double total = 0.0;
    for (const auto& order : it->second) {
        total += order->order_qty;
    }
    return total;
}

double Level3OrderBookState::get_bid_volume_within_bps(double reference_price, double bps) const {
    if (reference_price <= 0 || bps <= 0) {
        return 0.0;
    }

    double min_price = reference_price * (1.0 - bps / 10000.0);
    double total_volume = 0.0;

    for (const auto& pair : bids_by_price_) {
        if (pair.first >= min_price) {
            for (const auto& order : pair.second) {
                total_volume += order->order_qty;
            }
        } else {
            break;  // Sorted in descending order
        }
    }

    return total_volume;
}

double Level3OrderBookState::get_ask_volume_within_bps(double reference_price, double bps) const {
    if (reference_price <= 0 || bps <= 0) {
        return 0.0;
    }

    double max_price = reference_price * (1.0 + bps / 10000.0);
    double total_volume = 0.0;

    for (const auto& pair : asks_by_price_) {
        if (pair.first <= max_price) {
            for (const auto& order : pair.second) {
                total_volume += order->order_qty;
            }
        } else {
            break;  // Sorted in ascending order
        }
    }

    return total_volume;
}

double Level3OrderBookState::get_avg_bid_order_size() const {
    int order_count = get_total_bid_orders();
    if (order_count == 0) {
        return 0.0;
    }

    double total_volume = 0.0;
    for (const auto& pair : bids_by_price_) {
        for (const auto& order : pair.second) {
            total_volume += order->order_qty;
        }
    }

    return total_volume / order_count;
}

double Level3OrderBookState::get_avg_ask_order_size() const {
    int order_count = get_total_ask_orders();
    if (order_count == 0) {
        return 0.0;
    }

    double total_volume = 0.0;
    for (const auto& pair : asks_by_price_) {
        for (const auto& order : pair.second) {
            total_volume += order->order_qty;
        }
    }

    return total_volume / order_count;
}

void Level3OrderBookState::reset_event_counters() {
    add_count_ = 0;
    modify_count_ = 0;
    delete_count_ = 0;
}

Level3SnapshotMetrics Level3OrderBookState::calculate_metrics(const std::string& timestamp) const {
    Level3SnapshotMetrics metrics;
    metrics.timestamp = timestamp;
    metrics.symbol = symbol_;

    // Get best bid and ask
    double best_bid_price, best_bid_qty;
    double best_ask_price, best_ask_qty;

    bool has_bid = get_best_bid(best_bid_price, best_bid_qty);
    bool has_ask = get_best_ask(best_ask_price, best_ask_qty);

    if (has_bid) {
        metrics.best_bid = best_bid_price;
        metrics.best_bid_qty = best_bid_qty;
    }

    if (has_ask) {
        metrics.best_ask = best_ask_price;
        metrics.best_ask_qty = best_ask_qty;
    }

    // Calculate spread
    if (has_bid && has_ask) {
        metrics.spread = best_ask_price - best_bid_price;
        metrics.mid_price = (best_bid_price + best_ask_price) / 2.0;
        metrics.spread_bps = (metrics.spread / metrics.mid_price) * 10000.0;
    }

    // Calculate top 10 volume (sum first 10 price levels)
    int bid_level_count = 0;
    for (const auto& pair : bids_by_price_) {
        if (bid_level_count >= 10) break;
        for (const auto& order : pair.second) {
            metrics.bid_volume_top10 += order->order_qty;
        }
        bid_level_count++;
    }

    int ask_level_count = 0;
    for (const auto& pair : asks_by_price_) {
        if (ask_level_count >= 10) break;
        for (const auto& order : pair.second) {
            metrics.ask_volume_top10 += order->order_qty;
        }
        ask_level_count++;
    }

    // Calculate imbalance
    double total_volume = metrics.bid_volume_top10 + metrics.ask_volume_top10;
    if (total_volume > 0) {
        metrics.imbalance = (metrics.bid_volume_top10 - metrics.ask_volume_top10) / total_volume;
    }

    // Calculate depth within BPS
    if (has_bid && has_ask && metrics.mid_price > 0) {
        metrics.depth_10_bps = get_bid_volume_within_bps(metrics.mid_price, 10) +
                               get_ask_volume_within_bps(metrics.mid_price, 10);
        metrics.depth_25_bps = get_bid_volume_within_bps(metrics.mid_price, 25) +
                               get_ask_volume_within_bps(metrics.mid_price, 25);
        metrics.depth_50_bps = get_bid_volume_within_bps(metrics.mid_price, 50) +
                               get_ask_volume_within_bps(metrics.mid_price, 50);
    }

    // Order counts
    metrics.bid_order_count = get_total_bid_orders();
    metrics.ask_order_count = get_total_ask_orders();

    if (has_bid) {
        metrics.bid_orders_at_best = get_bid_orders_at_price(best_bid_price);
    }

    if (has_ask) {
        metrics.ask_orders_at_best = get_ask_orders_at_price(best_ask_price);
    }

    // Average order sizes
    metrics.avg_bid_order_size = get_avg_bid_order_size();
    metrics.avg_ask_order_size = get_avg_ask_order_size();

    // Event counts
    metrics.add_events = add_count_;
    metrics.modify_events = modify_count_;
    metrics.delete_events = delete_count_;

    // Flow metrics (rates per second would need time tracking)
    // For now, just copy event counts
    metrics.order_arrival_rate = add_count_;
    metrics.order_cancel_rate = delete_count_;

    return metrics;
}

} // namespace kraken
