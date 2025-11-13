# WebSocket Client Usage Patterns Guide

**Date**: 2025-11-12
**Applies to**: KrakenWebSocketClientBase and derived classes
**Related**: docs/memory_leak_pending_updates.md

---

## Overview

The Kraken WebSocket client supports multiple usage patterns for consuming real-time market data. This guide explains each pattern, when to use it, and important considerations.

---

## Pattern 1: Callback-Driven Mode (Recommended)

### When to Use

- ✅ Real-time processing requirements
- ✅ Event-driven architectures
- ✅ When you need immediate notification of updates
- ✅ Low-latency applications
- ✅ Most production use cases

### How It Works

Set a callback function that is invoked immediately when data arrives:

```cpp
#include "kraken_websocket_client_simdjson_v2.hpp"

using kraken::KrakenWebSocketClientSimdjsonV2;
using kraken::TickerRecord;

int main() {
    KrakenWebSocketClientSimdjsonV2 client;

    // Set callback - invoked immediately when data arrives
    client.set_update_callback([](const TickerRecord& record) {
        std::cout << record.pair << " = $" << record.last << std::endl;
        // Process data immediately
    });

    // Optional: Set connection and error callbacks
    client.set_connection_callback([](bool connected) {
        std::cout << (connected ? "Connected" : "Disconnected") << std::endl;
    });

    client.set_error_callback([](const std::string& error) {
        std::cerr << "Error: " << error << std::endl;
    });

    // Start client
    client.start({"BTC/USD", "ETH/USD"});

    // Main thread is free to do other work
    while (client.is_running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // Callbacks handle all data automatically
    }

    client.stop();
    return 0;
}
```

### Important Notes

- ⚠️ **Callback runs in worker thread**: Keep processing minimal or dispatch to another thread
- ⚠️ **Don't block**: Blocking in callback delays processing of subsequent updates
- ✅ **No memory leak**: `pending_updates_` is automatically managed
- ✅ **No need to call get_updates()**: Data delivered via callback

### Best Practices

```cpp
// ❌ BAD: Slow processing blocks worker thread
client.set_update_callback([](const TickerRecord& record) {
    // Don't do expensive operations here!
    save_to_database(record);  // This blocks the worker thread
    send_to_api(record);       // This delays next update
});

// ✅ GOOD: Fast callback + async processing
std::queue<TickerRecord> work_queue;
std::mutex queue_mutex;

client.set_update_callback([&](const TickerRecord& record) {
    // Quick: just queue the data
    std::lock_guard<std::mutex> lock(queue_mutex);
    work_queue.push(record);
});

// Separate processing thread
std::thread worker([&]() {
    while (running) {
        TickerRecord record;
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (work_queue.empty()) continue;
            record = work_queue.front();
            work_queue.pop();
        }
        // Do expensive processing here
        save_to_database(record);
    }
});
```

---

## Pattern 2: Polling Mode

### When to Use

- ✅ Batch processing requirements
- ✅ You control the processing loop timing
- ✅ Integration with existing polling architecture
- ✅ Testing and debugging (easier to step through)

### How It Works

Periodically call `get_updates()` to retrieve accumulated data:

```cpp
#include "kraken_websocket_client_simdjson_v2.hpp"

int main() {
    KrakenWebSocketClientSimdjsonV2 client;

    // No callback - use polling instead
    client.start({"BTC/USD", "ETH/USD"});

    while (client.is_running()) {
        // Wait before polling
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Get all updates since last poll
        auto updates = client.get_updates();

        if (!updates.empty()) {
            std::cout << "Received " << updates.size() << " updates" << std::endl;

            for (const auto& record : updates) {
                std::cout << record.pair << " = $" << record.last << std::endl;
                // Process each update
            }
        }
    }

    client.stop();
    return 0;
}
```

### Important Notes

- ⚠️ **Poll frequency matters**: Data buffered between polls consumes memory
- ⚠️ **Beware of flush interval**: See Pattern 3 for details
- ✅ **Control timing**: You decide when to process updates
- ✅ **Thread safety**: `get_updates()` is thread-safe

### Best Practices

```cpp
// ✅ GOOD: Regular polling (1-5 seconds typical)
while (running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto updates = client.get_updates();
    process_batch(updates);
}

// ❌ BAD: Slow polling causes memory buildup
while (running) {
    std::this_thread::sleep_for(std::chrono::minutes(5));  // Too slow!
    auto updates = client.get_updates();  // Large batch, memory buildup
}
```

---

## Pattern 3: Polling + Periodic Flushing (Advanced)

### When to Use

- ✅ You want both polling AND automatic backup to file
- ✅ Long-running data collection with persistence
- ⚠️ **Requires careful timing coordination**

### How It Works

Combine polling with automatic file writes:

```cpp
#include "kraken_websocket_client_simdjson_v2.hpp"

int main() {
    KrakenWebSocketClientSimdjsonV2 client;

    // Configure periodic flushing to file
    client.set_output_file("backup.csv");
    client.set_flush_interval(std::chrono::seconds(30));  // Flush every 30s

    client.start({"BTC/USD", "ETH/USD"});

    while (client.is_running()) {
        // IMPORTANT: Poll MORE frequently than flush interval!
        std::this_thread::sleep_for(std::chrono::seconds(10));  // 10s < 30s

        auto updates = client.get_updates();
        process_updates(updates);
    }

    // Final flush
    client.flush();
    client.stop();
    return 0;
}
```

### Critical Warning ⚠️

**During periodic flush, `pending_updates_` is automatically cleared to prevent memory leaks in callback mode.**

This means if you poll **slower** than the flush interval, you may lose data:

```cpp
// ❌ DANGEROUS: Polling slower than flush
client.set_flush_interval(std::chrono::seconds(30));

while (running) {
    std::this_thread::sleep_for(std::chrono::seconds(60));  // 60s > 30s
    auto updates = client.get_updates();
    // MAY BE EMPTY! Cleared by flush at 30s mark
}
```

### Best Practices

**Rule of Thumb**: Poll at least every `(flush_interval / 2)`:

```cpp
// ✅ SAFE: Poll 3x more frequently than flush
auto flush_interval = std::chrono::seconds(30);
auto poll_interval = std::chrono::seconds(10);  // 30 / 3

client.set_flush_interval(flush_interval);

while (running) {
    std::this_thread::sleep_for(poll_interval);
    auto updates = client.get_updates();
    process_updates(updates);
}
```

**Alternative: Flush on demand**:

```cpp
// ✅ SAFE: Explicit control over flush timing
client.set_output_file("data.csv");
// Don't set automatic flush interval

while (running) {
    std::this_thread::sleep_for(std::chrono::seconds(60));
    auto updates = client.get_updates();
    process_updates(updates);

    // Flush after processing
    client.flush();
}
```

---

## Pattern 4: Callback + Periodic Flushing (Common)

### When to Use

- ✅ Real-time processing + data persistence
- ✅ Most production deployments
- ✅ Long-running data collection

### How It Works

Process data via callback while automatically backing up to file:

```cpp
#include "kraken_websocket_client_simdjson_v2.hpp"

int main() {
    KrakenWebSocketClientSimdjsonV2 client;

    // Real-time processing via callback
    client.set_update_callback([](const TickerRecord& record) {
        // Process immediately
        update_dashboard(record);
        check_trading_signals(record);
    });

    // Automatic backup to file
    client.set_output_file("kraken_data.csv");
    client.set_flush_interval(std::chrono::seconds(30));
    client.set_memory_threshold(10 * 1024 * 1024);  // 10 MB

    // Optional: Enable segmentation for long runs
    client.set_segment_mode(kraken::SegmentMode::HOURLY);

    client.start({"BTC/USD", "ETH/USD", "SOL/USD"});

    std::cout << "Collecting data... Press Ctrl+C to stop" << std::endl;

    while (client.is_running()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Optional: Show status
        std::cout << "Flushes: " << client.get_flush_count()
                  << " | Memory: " << (client.get_current_memory_usage() / 1024 / 1024) << " MB"
                  << std::endl;
    }

    // Final flush on shutdown
    client.flush();
    client.stop();

    return 0;
}
```

### Important Notes

- ✅ **No memory leak**: `pending_updates_` cleared during flush
- ✅ **Data persisted**: Automatic backup every 30s (or when memory threshold reached)
- ✅ **Real-time processing**: Callback for immediate notification
- ✅ **No need to call get_updates()**: Data already delivered via callback

### Best Practices

```cpp
// Configure flush parameters based on update frequency
if (high_frequency_pairs) {
    client.set_flush_interval(std::chrono::seconds(10));   // Flush more often
    client.set_memory_threshold(5 * 1024 * 1024);          // Lower threshold
} else {
    client.set_flush_interval(std::chrono::seconds(60));   // Less frequent
    client.set_memory_threshold(20 * 1024 * 1024);         // Higher threshold
}

// For 24/7 operations, use segmentation
client.set_segment_mode(kraken::SegmentMode::HOURLY);  // New file every hour
// Results in files like: data.20251112_10.csv, data.20251112_11.csv, ...
```

---

## Comparison Matrix

| Feature | Callback | Polling | Callback + Flush | Polling + Flush |
|---------|----------|---------|------------------|-----------------|
| **Real-time processing** | ✅ Immediate | ⚠️ Delayed | ✅ Immediate | ⚠️ Delayed |
| **Memory efficient** | ✅ Yes | ⚠️ Depends on poll freq | ✅ Yes | ⚠️ Risk if slow |
| **Data persistence** | ❌ No (unless flush) | ❌ No (unless flush) | ✅ Yes | ✅ Yes |
| **Complexity** | Low | Low | Medium | High |
| **Thread safety concerns** | ⚠️ Callback in worker | ✅ Main thread | ⚠️ Callback in worker | ✅ Main thread |
| **Best for** | Real-time apps | Batch processing | Production 24/7 | Testing/debugging |

---

## Common Pitfalls

### Pitfall 1: Mixing Patterns Incorrectly

```cpp
// ❌ BAD: Setting callback but also polling (wastes resources)
client.set_update_callback([](auto& r) { process(r); });
client.start(symbols);

while (running) {
    auto updates = client.get_updates();  // Unnecessary! Callback already delivered data
    // This will be empty or redundant
}
```

### Pitfall 2: Slow Polling + Flush

```cpp
// ❌ BAD: Polling slower than flush interval
client.set_flush_interval(std::chrono::seconds(30));
client.start(symbols);

while (running) {
    std::this_thread::sleep_for(std::chrono::minutes(1));  // 60s > 30s
    auto updates = client.get_updates();  // May be empty!
}
```

### Pitfall 3: Blocking Callback

```cpp
// ❌ BAD: Blocking operation in callback
client.set_update_callback([](const TickerRecord& record) {
    std::this_thread::sleep_for(std::chrono::seconds(5));  // Blocks worker thread!
    save_to_database(record);
});
```

### Pitfall 4: Not Calling flush() on Shutdown

```cpp
// ❌ BAD: Losing final data
client.set_output_file("data.csv");
client.start(symbols);
// ... run for a while ...
client.stop();  // Missing: client.flush()
// Data accumulated since last automatic flush is lost!
```

---

## Recommendations by Use Case

### High-Frequency Trading (HFT)

```cpp
// Callback for minimum latency
client.set_update_callback([](const TickerRecord& record) {
    // Ultra-fast: just signal condition variable
    signal_trading_engine(record);
});

// No file I/O in critical path
// Optionally log to separate thread
```

### Data Collection / Research

```cpp
// Callback + periodic flush for reliability
client.set_output_file("research_data.csv");
client.set_flush_interval(std::chrono::seconds(30));
client.set_segment_mode(kraken::SegmentMode::DAILY);

client.set_update_callback([](const TickerRecord& record) {
    update_live_chart(record);  // Optional real-time display
});
```

### Batch Analytics

```cpp
// Polling for controlled batch processing
while (running) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    auto updates = client.get_updates();

    if (updates.size() >= 100) {
        run_analytics_batch(updates);
    }
}
```

### 24/7 Production Monitoring

```cpp
// Callback + flush + segmentation for reliability
client.set_output_file("monitoring.csv");
client.set_flush_interval(std::chrono::seconds(30));
client.set_segment_mode(kraken::SegmentMode::HOURLY);

client.set_update_callback([](const TickerRecord& record) {
    update_monitoring_dashboard(record);
    check_price_alerts(record);
});

client.set_error_callback([](const std::string& error) {
    log_error(error);
    send_alert_to_ops(error);
});
```

---

## FAQ

### Q: Can I use both callback and polling?

**A**: Technically yes, but not recommended. The callback delivers data immediately, so polling `get_updates()` afterward will return empty or redundant data. Choose one pattern.

### Q: What happens if I don't call get_updates() in callback mode?

**A**: Prior to v1.x.y, this caused a memory leak. Now, `pending_updates_` is automatically cleared during periodic flush to prevent this. No action needed.

### Q: How do I avoid data loss with polling + flush?

**A**: Poll more frequently than your flush interval. Rule of thumb: `poll_interval <= flush_interval / 2`

### Q: Can I change patterns dynamically?

**A**: Not recommended. Set callbacks and configure flush settings before calling `start()`. Changing during operation can lead to unexpected behavior.

### Q: What's the overhead of unused pending_updates_?

**A**: In callback mode with auto-flush, memory overhead is now minimal. The buffer is periodically cleared even if not polled.

---

## See Also

- `docs/memory_leak_pending_updates.md` - Technical details on memory leak fix
- `docs/code_review_websocket_client_base.md` - Full code review
- `examples/example_callback_driven.cpp` - Callback pattern example
- `examples/example_simple_polling.cpp` - Polling pattern example
- `examples/retrieve_kraken_live_data_level1.cpp` - Production pattern with flush

---

## Version History

- **v1.x.y** (2025-11-12): Fixed memory leak by auto-clearing pending_updates_ during flush
- **v1.0.0**: Initial release with callback and polling patterns
