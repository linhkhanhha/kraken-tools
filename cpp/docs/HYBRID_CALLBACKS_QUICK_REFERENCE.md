# Hybrid Callbacks Quick Reference

Quick cheat sheet for using hybrid callbacks in the Kraken WebSocket client.

---

## TL;DR

**Default Mode** (recommended - fast enough for most uses):
```cpp
#include "lib/kraken_websocket_client_simdjson_v2_hybrid.hpp"
KrakenWebSocketClientSimdjsonV2Hybrid client;
client.set_update_callback([](const TickerRecord& r) { /* ... */ });
```

**Stateless Mode** (0ns overhead - use global state):
```cpp
static std::atomic<int> g_count{0};
auto callback = [](const TickerRecord& r) { g_count++; /* ... */ };
KrakenWebSocketClientSimdjsonV2Hybrid client;
client.set_update_callback(callback);
```

---

## Import

```cpp
// Hybrid version (recommended)
#include "lib/kraken_websocket_client_simdjson_v2_hybrid.hpp"
using namespace kraken;

// Or nlohmann/json version
#include "lib/kraken_websocket_client_v2_hybrid.hpp"
```

---

## Creating Clients

### Default Mode (std::function)

```cpp
// Easiest - works like original
KrakenWebSocketClientSimdjsonV2Hybrid client;

// Can change callbacks anytime
client.set_update_callback([](const TickerRecord& r) {
    std::cout << r.symbol << ": " << r.last << std::endl;
});
```

**Performance**: ~4ns per callback (already very fast!)
**Use When**: Most applications - this is the recommended approach

---

### Stateless Performance Mode

```cpp
// Use global/static state instead of captures
static std::atomic<int> g_count{0};

// Stateless callback (no captures)
auto callback = [](const TickerRecord& r) {
    g_count++;  // Access global state
};

// Default mode works fine for stateless callbacks
KrakenWebSocketClientSimdjsonV2Hybrid client;
client.set_update_callback(callback);
```

**Performance**: ~0ns per callback (completely optimized away)
**Use When**: Need absolute maximum performance, can use global/static state

---

### Stateless Performance Mode

```cpp
// Global state
static std::atomic<uint64_t> g_count{0};

// Stateless callback (no captures)
auto callback = [](const TickerRecord& r) {
    g_count++;
};

KrakenWebSocketClientSimdjsonV2Hybrid<decltype(callback)> client;
client.set_update_callback(callback);
```

**Performance**: ~0ns (compiler optimizes aggressively)
**Use When**: Ultimate performance, can use global state

---

## Setting Callbacks

### Update Callback (Hot Path)

```cpp
// Default mode
client.set_update_callback([](const TickerRecord& record) {
    // Called for every ticker update (1000s/sec)
});

// Performance mode - define first
auto cb = [](const TickerRecord& r) { /* ... */ };
// Then create client with decltype(cb)
```

**Frequency**: 1,000-50,000 calls/sec
**Optimization**: Use template for zero overhead

---

### Connection Callback (Cold Path)

```cpp
// Always std::function (flexibility OK for rare events)
client.set_connection_callback([](bool connected) {
    if (connected) {
        std::cout << "Connected!" << std::endl;
    }
});
```

**Frequency**: <10 calls/session
**Optimization**: Not needed (rare event)

---

### Error Callback (Cold Path)

```cpp
// Always std::function
client.set_error_callback([](const std::string& error) {
    std::cerr << "Error: " << error << std::endl;
});
```

**Frequency**: <10 calls/session
**Optimization**: Not needed (rare event)

---

## Starting and Stopping

```cpp
// Subscribe to symbols
std::vector<std::string> symbols = {"BTC/USD", "ETH/USD"};
client.start(symbols);

// ... process updates ...

// Stop when done
client.stop();
```

**Important**: Set all callbacks BEFORE calling `start()` in performance mode.

---

## Capturing State

### Default Mode (Easy)

```cpp
// Capture by reference
int count = 0;
client.set_update_callback([&count](const TickerRecord& r) {
    count++;  // Modify outer variable
});
```

Works naturally with captures.

---

### Performance Mode (Need Care)

```cpp
// Capture in callback definition
int count = 0;
auto callback = [&count](const TickerRecord& r) {
    count++;
};

// Pass callback type to client
KrakenWebSocketClientSimdjsonV2Hybrid<decltype(callback)> client;
client.set_update_callback(callback);
```

Callback type includes captures, part of template.

---

### Class Members

```cpp
class MyProcessor {
    int count_ = 0;

public:
    void run() {
        // Capture 'this'
        auto callback = [this](const TickerRecord& r) {
            count_++;  // Access member variable
            process(r);
        };

        KrakenWebSocketClientSimdjsonV2Hybrid<decltype(callback)> client;
        client.set_update_callback(callback);
        client.start({"BTC/USD"});
    }

    void process(const TickerRecord& r) {
        // Member function
    }
};
```

---

## Performance Comparison

**Actual Benchmark Results** (1 million callbacks):

| Mode | Overhead | Memory | When to Use |
|------|----------|--------|-------------|
| Default (std::function) | ~4ns | 32-48 bytes | **Recommended - use this** |
| Template (with capture) | ~3.5ns | 8 bytes | Not much benefit |
| Stateless | ~0ns | 0 bytes | **Maximum performance** |

**Key Insight**: std::function is already fast (~4ns). Real optimization comes from stateless callbacks (0ns).

---

## Common Patterns

### Pattern 1: Simple Console Output

```cpp
// Default mode is fine
KrakenWebSocketClientSimdjsonV2Hybrid client;
client.set_update_callback([](const TickerRecord& r) {
    std::cout << r.symbol << ": " << r.last << std::endl;
});
```

---

### Pattern 2: Update Counter

```cpp
// Default mode
std::atomic<int> count{0};
KrakenWebSocketClientSimdjsonV2Hybrid client;
client.set_update_callback([&count](const TickerRecord& r) {
    count++;
});
```

---

### Pattern 3: Data Aggregation (Stateless for Max Performance)

```cpp
// Global state for zero-overhead callbacks
static std::atomic<uint64_t> g_total_volume{0};

// Stateless callback - compiler optimizes to 0ns
auto callback = [](const TickerRecord& r) {
    g_total_volume += static_cast<uint64_t>(r.volume);
};

// Default mode works fine
KrakenWebSocketClientSimdjsonV2Hybrid client;
client.set_update_callback(callback);
```

---

### Pattern 4: Queue for Processing

```cpp
// Thread-safe queue
std::queue<TickerRecord> queue;
std::mutex queue_mutex;

// Default mode (flexibility needed)
KrakenWebSocketClientSimdjsonV2Hybrid client;
client.set_update_callback([&queue, &queue_mutex](const TickerRecord& r) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    queue.push(r);
});
```

---

## When to Optimize

### Reality Check

**Actual benchmark results** show:
- std::function: **3.8 ns/call** (already very fast!)
- For 10K updates/sec: only **40 μs/sec** overhead
- For 50K updates/sec: only **200 μs/sec** overhead

**Conclusion**: std::function is fast enough for almost all use cases.

### Step 1: Profile First

```bash
# Run with profiler
perf record -g ./my_app
perf report
```

### Step 2: Only Optimize If Bottleneck

If profiling shows callback overhead is significant:
1. Rewrite callback to be stateless (no captures)
2. Use global/static state
3. Compiler will optimize to 0ns overhead

**Note**: Default mode with std::function works fine for stateless callbacks too!

---

## Build Commands

```bash
# CMake configuration
cd cpp/build
cmake ..

# Build specific example
cmake --build . --target example_hybrid_callbacks

# Run benchmark
./example_hybrid_callbacks
```

---

## Troubleshooting

### Problem: "record.symbol not found"

**Solution**: Use `record.pair` not `record.symbol`:
```cpp
// ❌ Wrong
std::cout << record.symbol;

// ✅ Right
std::cout << record.pair;
```

---

### Problem: "record.volume_24h not found"

**Solution**: Use `record.volume` not `record.volume_24h`:
```cpp
// ❌ Wrong
total += record.volume_24h;

// ✅ Right
total += record.volume;
```

---

### Problem: "Lambda with captures can't be default constructed"

**Solution**: Use default mode (std::function) for capturing lambdas:
```cpp
// ✅ This works - capturing lambda with default mode
int count = 0;
KrakenWebSocketClientSimdjsonV2Hybrid client;
client.set_update_callback([&count](const TickerRecord& r) {
    count++;
});
```

---

### Problem: "Not seeing performance improvement"

**Solution**: std::function is already fast (~4ns). For 0ns overhead, use stateless callbacks:
```cpp
// Global state
static std::atomic<int> g_count{0};

// Stateless callback (compiler optimizes to 0ns)
auto callback = [](const TickerRecord& r) { g_count++; };
client.set_update_callback(callback);
```

---

## Decision Tree

```
Can you write stateless callbacks (no captures)?
├─ YES → Use stateless callbacks (0ns overhead)
│   └─ Default mode works fine for this!
└─ NO (need to capture state)
   └─ Use default mode (std::function)
      └─ 4ns overhead is negligible for almost all cases
```

**Recommendation**: Start with default mode. Only optimize to stateless if profiling shows callback overhead is a bottleneck.

---

## Complete Example (Default Mode)

```cpp
#include "lib/kraken_websocket_client_simdjson_v2_hybrid.hpp"
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>

using namespace kraken;

int main() {
    KrakenWebSocketClientSimdjsonV2Hybrid client;

    std::atomic<int> count{0};

    // Set callbacks
    client.set_update_callback([&count](const TickerRecord& r) {
        count++;
        if (count <= 5) {
            std::cout << r.symbol << ": " << r.last << std::endl;
        }
    });

    client.set_connection_callback([](bool connected) {
        std::cout << (connected ? "Connected" : "Disconnected") << std::endl;
    });

    // Start
    client.start({"BTC/USD", "ETH/USD"});

    // Run for 10 seconds
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Stop
    client.stop();

    std::cout << "Total updates: " << count << std::endl;
    return 0;
}
```

---

## Complete Example (Stateless Performance Mode)

```cpp
#include "lib/kraken_websocket_client_simdjson_v2_hybrid.hpp"
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>

using namespace kraken;

// Global state for stateless callbacks
static std::atomic<int> g_count{0};

int main() {
    // Stateless callback - compiler optimizes to 0ns
    auto fast_callback = [](const TickerRecord& r) {
        g_count++;
        if (g_count <= 5) {
            std::cout << r.pair << ": " << r.last << std::endl;
        }
    };

    // Default mode works fine for stateless callbacks
    KrakenWebSocketClientSimdjsonV2Hybrid client;

    // Set callbacks
    client.set_update_callback(fast_callback);

    client.set_connection_callback([](bool connected) {
        std::cout << (connected ? "Connected" : "Disconnected") << std::endl;
    });

    // Start
    client.start({"BTC/USD", "ETH/USD", "SOL/USD"});

    // Run for 10 seconds
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Stop
    client.stop();

    std::cout << "Total updates: " << g_count << std::endl;
    std::cout << "Performance: Maximum (0ns overhead - stateless)" << std::endl;
    return 0;
}
```

---

## See Also

- **[HYBRID_CALLBACKS_SUMMARY.md](HYBRID_CALLBACKS_SUMMARY.md)** - Detailed overview
- **[callback_mechanisms.md](callback_mechanisms.md)** - All callback approaches analyzed
- **[hybrid_implementation_changes.md](hybrid_implementation_changes.md)** - Migration guide
- **[EXAMPLES_README.md](EXAMPLES_README.md)** - General examples

---

**Last Updated**: October 2025
