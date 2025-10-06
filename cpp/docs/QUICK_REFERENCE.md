# Quick Reference Guide

## Available Implementations

### 1. Blocking Version (Simple)
**File**: `query_live_data_v2_refactored.cpp`

**Usage**:
```bash
./build/query_live_data_v2
```

**When to use**: Simple standalone applications

**Example**:
```cpp
int main() {
    ws_client.run();  // Blocks here
    return 0;
}
```

---

### 2. Non-blocking Version (Advanced)
**File**: `query_live_data_v2_nonblocking.cpp`

**Usage**:
```bash
./build/query_live_data_v2_nonblocking
```

**When to use**: Integration with existing systems, GUI apps

**Example - Polling**:
```cpp
KrakenWebSocketClient client;
client.start({"BTC/USD"});

while (client.is_running()) {
    auto updates = client.get_updates();
    // Process updates...
}
```

**Example - Callbacks**:
```cpp
KrakenWebSocketClient client;

client.set_update_callback([](const TickerRecord& r) {
    std::cout << r.pair << ": " << r.last << std::endl;
});

client.start({"BTC/USD"});
// Main thread free to do other work
```

---

### 3. Standalone Demo (Minimal)
**File**: `query_live_data_v2_standalone_refactored.cpp`

**Usage**:
```bash
./build/query_live_data_v2_standalone
```

**When to use**: Learning, testing, no actual connection needed

---

## Build Commands

```bash
cd /export1/rocky/dev/kraken/cpp

# Build everything
cmake -B build -S .
cmake --build build

# Build outputs:
./build/query_live_data_v2                # Blocking
./build/query_live_data_v2_nonblocking   # Non-blocking
./build/query_live_data_v2_standalone    # Demo
```

## Common Library API

### TickerRecord
```cpp
namespace kraken {
    struct TickerRecord {
        std::string timestamp;
        std::string pair;
        std::string type;
        double bid, bid_qty;
        double ask, ask_qty;
        double last, volume, vwap;
        double low, high;
        double change, change_pct;
    };
}
```

### Utils Class
```cpp
namespace kraken {
    class Utils {
        // Get current timestamp
        static std::string get_utc_timestamp();

        // Save records to CSV
        static void save_to_csv(const std::string& filename,
                               const std::vector<TickerRecord>& records);

        // Print record to console
        static void print_record(const TickerRecord& record);

        // Print CSV header
        static void print_csv_header();
    };
}
```

### SimpleJsonParser (for standalone)
```cpp
namespace kraken {
    class SimpleJsonParser {
        static std::string extract_string(const std::string& json,
                                          const std::string& key);
        static double extract_number(const std::string& json,
                                     const std::string& key);
        static bool contains(const std::string& json,
                            const std::string& key);
    };
}
```

## Non-blocking Client API

### Starting/Stopping
```cpp
KrakenWebSocketClient client;

// Start (non-blocking, returns immediately)
client.start({"BTC/USD", "ETH/USD"});

// Stop and cleanup
client.stop();
```

### Status Checks
```cpp
bool running = client.is_running();      // Is client thread running?
bool connected = client.is_connected();  // Is WebSocket connected?
size_t pending = client.pending_count(); // How many updates queued?
```

### Data Access - Polling
```cpp
// Get all pending updates (clears buffer)
std::vector<TickerRecord> updates = client.get_updates();

// Get full history
std::vector<TickerRecord> history = client.get_history();
```

### Data Access - Callbacks
```cpp
// Set update callback
client.set_update_callback([](const TickerRecord& record) {
    // Called from worker thread!
    std::cout << record.pair << ": " << record.last << std::endl;
});

// Set connection callback
client.set_connection_callback([](bool connected) {
    std::cout << (connected ? "Connected" : "Disconnected") << std::endl;
});
```

### Saving Data
```cpp
// Save all history to CSV
client.save_to_csv("output.csv");
```

## Integration Patterns

### Pattern 1: Simple Polling Loop
```cpp
KrakenWebSocketClient client;
client.start(symbols);

while (client.is_running()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto updates = client.get_updates();
    for (const auto& update : updates) {
        process(update);
    }
}
```

### Pattern 2: Event-driven with Callbacks
```cpp
KrakenWebSocketClient client;

client.set_update_callback([](const TickerRecord& r) {
    // Immediate notification
    process(r);
});

client.start(symbols);

while (client.is_running()) {
    do_other_work();
}
```

### Pattern 3: Hybrid (Notification + Polling)
```cpp
std::atomic<bool> has_updates{false};

client.set_update_callback([&](const TickerRecord&) {
    has_updates = true;  // Just set flag
});

client.start(symbols);

while (client.is_running()) {
    if (has_updates.exchange(false)) {
        auto updates = client.get_updates();
        process_batch(updates);
    }
    do_other_work();
}
```

## Documentation Files

| File | Purpose |
|------|---------|
| **README.md** | Setup and installation |
| **README_REFACTORED.md** | Refactored code guide |
| **BLOCKING_VS_NONBLOCKING.md** | Detailed comparison |
| **DEVELOPMENT_NOTES.md** | Build issues, troubleshooting |
| **REFACTORING.md** | Refactoring process |
| **FLOAT_VS_DOUBLE_ANALYSIS.md** | Precision analysis |
| **SUMMARY.md** | Project overview |
| **QUICK_REFERENCE.md** | This file |

## Common Tasks

### Change Symbols
```cpp
// Blocking version - edit source file
// query_live_data_v2_refactored.cpp, line ~115
{"symbol", {"BTC/USD", "ETH/USD", "SOL/USD"}},

// Non-blocking version - pass to start()
client.start({"BTC/USD", "ETH/USD", "XRP/USD"});
```

### Change Output File
```cpp
// Blocking - edit signal_handler
Utils::save_to_csv("my_output.csv", ticker_history);

// Non-blocking
client.save_to_csv("my_output.csv");
```

### Add Custom Processing
```cpp
// Blocking - edit on_message()
void on_message(...) {
    // ... create record
    custom_process(record);  // Add here
    ticker_history.push_back(record);
}

// Non-blocking - use callback
client.set_update_callback([](const TickerRecord& r) {
    custom_process(r);
});
```

## Troubleshooting

### Build Issues
```bash
# Missing boost
sudo yum install boost-devel

# Missing websocketpp
git clone https://github.com/zaphoyd/websocketpp.git

# Missing nlohmann/json
mkdir -p include/nlohmann
wget -O include/nlohmann/json.hpp https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp

# Rebuild
cmake -B build -S .
cmake --build build
```

### Runtime Issues
```bash
# Connection timeout
# - Check internet connectivity
# - Verify firewall allows outbound HTTPS

# Segmentation fault
# - Check callback thread safety
# - Verify client lifetime (don't delete while running)

# No data received
# - Check symbols are valid
# - Verify subscription succeeded (check console output)
```

## Performance Tips

### For Blocking Version
- Minimize work in callbacks
- Use efficient data structures
- Consider buffering before writing to disk

### For Non-blocking Version
- Keep callbacks lightweight
- Process data in main thread, not callback
- Use move semantics to avoid copies
- Poll at appropriate frequency (100-1000ms typical)

## Thread Safety Notes

### Blocking Version
- Single-threaded, no synchronization needed
- All callbacks run in main thread

### Non-blocking Version
- Callbacks run in worker thread!
- Use provided thread-safe methods
- Don't access internal state directly
- Keep callback processing minimal

## Quick Decision Tree

```
Need to integrate with existing system?
├─ Yes → Use non-blocking
└─ No
   ├─ Simple standalone tool?
   │  └─ Yes → Use blocking
   └─ GUI application?
      └─ Yes → Use non-blocking
```

## Examples by Use Case

### Use Case: Price Monitor (CLI)
→ **Use blocking version**

### Use Case: Trading Bot
→ **Use non-blocking version**

### Use Case: Qt/GTK Application
→ **Use non-blocking version**

### Use Case: Multi-exchange Aggregator
→ **Use non-blocking version** (multiple instances)

### Use Case: Historical Data Logger
→ **Use blocking version**

### Use Case: Web Dashboard Backend
→ **Use non-blocking version**
