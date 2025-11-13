# Memory Leak Issue: pending_updates_ Unbounded Growth

**Date**: 2025-11-12
**Severity**: HIGH - Memory Leak
**Discovered By**: Code Review
**Status**: DOCUMENTED (Not Yet Fixed)

---

## Executive Summary

The `pending_updates_` vector in `KrakenWebSocketClientBase` grows unbounded when `get_updates()` is not called, leading to a memory leak in callback-driven and periodic-flush usage patterns. This affects **11 out of 14 example programs** and likely most production use cases.

**Impact**:
- Memory growth: ~7-172 MB/day depending on update frequency
- Affects long-running sessions (hours to days)
- Worse for high-frequency trading pairs or many subscribed symbols

---

## Problem Description

### Current Implementation

**Data Storage in add_record()** (lib/kraken_websocket_client_base.hpp:511-533):
```cpp
void add_record(const TickerRecord& record) {
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        ticker_history_.push_back(record);      // Cleared by periodic flush
        pending_updates_.push_back(record);      // ONLY cleared by get_updates()!
        this->check_and_flush();
    }

    // Call user callback (outside data lock)
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (update_callback_) {
            update_callback_(record);
        }
    }
}
```

**The Only Clear Location** (lib/kraken_websocket_client_base.hpp:284-289):
```cpp
std::vector<TickerRecord> get_updates() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    std::vector<TickerRecord> updates = std::move(pending_updates_);
    pending_updates_.clear();  // ← ONLY place it's cleared!
    return updates;
}
```

### The Leak

**ticker_history_**:
- Grows with each record
- Periodically cleared by `perform_flush()` (every 30s by default)
- Bounded growth: ~30 seconds worth of data

**pending_updates_**:
- Grows with each record
- ONLY cleared by calling `get_updates()`
- If `get_updates()` never called → **unbounded growth** → memory leak

---

## Usage Pattern Analysis

### Pattern 1: Callback-Driven ❌ (LEAKS)

**Example**: `example_callback_driven.cpp`, `example_hybrid_callbacks.cpp`

```cpp
client.set_update_callback([](const TickerRecord& record) {
    // Process data immediately
    std::cout << record.pair << " = $" << record.last << std::endl;
});

client.start({"BTC/USD", "ETH/USD"});

while (client.is_running()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // Never calls get_updates() → LEAK!
}
```

**Why it leaks**:
- User processes data via callback
- No need to call `get_updates()` (data already delivered)
- `pending_updates_` grows forever

**Affected Examples**: 8 out of 14
- `example_callback_driven.cpp`
- `example_hybrid_callbacks.cpp`
- `retrieve_kraken_live_data_level1.cpp`
- `retrieve_kraken_live_data_level2.cpp`
- `retrieve_kraken_live_data_level3.cpp`
- `example_simdjson_comparison.cpp`
- `query_live_data_v2.cpp`
- `query_live_data_v2_standalone.cpp`

---

### Pattern 2: Periodic Flushing Only ❌ (LEAKS)

**Example**: `retrieve_kraken_live_data_level1.cpp`

```cpp
client.set_output_file("data.csv");
client.set_flush_interval(std::chrono::seconds(30));
client.start(symbols);

while (client.is_running()) {
    // Just monitor, data auto-flushes to file
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // Never calls get_updates() → LEAK!
}

client.flush();  // Final flush
```

**Why it leaks**:
- Data automatically written to file every 30s
- `ticker_history_` cleared by `perform_flush()`
- `pending_updates_` NEVER cleared → grows unbounded

**Affected Examples**: 3 out of 14 (plus Pattern 1 overlap)

---

### Pattern 3: Polling ✅ (NO LEAK)

**Example**: `example_simple_polling.cpp`, `example_integration.cpp`

```cpp
client.start(symbols);

while (client.is_running()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto updates = client.get_updates();  // ← Clears pending_updates_
    for (const auto& record : updates) {
        std::cout << record.pair << " = $" << record.last << std::endl;
    }
}
```

**Why no leak**:
- Regular calls to `get_updates()` clear the vector
- Intended design for polling pattern

**Affected Examples**: 3 out of 14
- `example_simple_polling.cpp`
- `example_integration.cpp`
- `example_integration_cond.cpp`

---

## Memory Impact Analysis

### Memory Growth Rate

**Per-Record Memory**:
```cpp
struct TickerRecord {
    std::string timestamp;    // ~32 bytes
    std::string pair;         // ~16 bytes
    std::string type;         // ~16 bytes
    double bid, ask, last;    // 24 bytes
    double bid_qty, ask_qty;  // 16 bytes
    double volume, vwap;      // 16 bytes
    double low, high;         // 16 bytes
    double change, change_pct;// 16 bytes
};
// Estimated: ~152 bytes + overhead = ~200 bytes per record
```

**Update Frequency by Use Case**:

| Use Case | Updates/sec | Memory/hour | Memory/day |
|----------|-------------|-------------|------------|
| Single low-freq pair (BTC/USD) | 1-2 | 1.4 MB | 34 MB |
| Multiple pairs (10) | 10 | 14 MB | 336 MB |
| High-frequency pairs | 10-100 | 14-140 MB | 0.3-3.4 GB |
| Order book Level 2 | 100-1000 | 140 MB-1.4 GB | 3.4-34 GB |

### Real-World Scenarios

#### Scenario 1: Development Testing (Low Impact)
- Run time: 5 minutes
- Pairs: 3 (BTC/USD, ETH/USD, SOL/USD)
- Update rate: ~5/sec
- **Memory leak**: ~1.5 MB
- **Impact**: Negligible

#### Scenario 2: Production Monitoring (Medium Impact)
- Run time: 24 hours
- Pairs: 20 crypto pairs
- Update rate: ~20/sec
- **Memory leak**: ~672 MB/day
- **Impact**: Noticeable but manageable

#### Scenario 3: High-Frequency Trading (Critical Impact)
- Run time: 7 days
- Pairs: 50 pairs with order book
- Update rate: ~500/sec (order book updates)
- **Memory leak**: ~100 GB/week
- **Impact**: CRITICAL - OOM crash

---

## Proposed Solutions

### Solution 1: Clear pending_updates_ During Flush

**Approach**: Auto-clear `pending_updates_` when `ticker_history_` is flushed to disk.

**Implementation**:
```cpp
void perform_flush() {
    if (ticker_history_.empty()) {
        return;
    }

    if (!output_file_.is_open()) {
        return;
    }

    // Write header only on first write
    if (!csv_header_written_) {
        output_file_ << "timestamp,pair,type,...\n";
        csv_header_written_ = true;
    }

    // Write data
    for (const auto& record : ticker_history_) {
        output_file_ << record.timestamp << "," << ...;
    }
    output_file_.flush();

    // Clear buffers
    ticker_history_.clear();
    ticker_history_.reserve(RECORD_BUFFER_INITIAL_CAPACITY);

    // NEW: Also clear pending_updates_ since data is persisted
    pending_updates_.clear();
}
```

**Pros**:
- ✅ Simple one-line fix
- ✅ Fixes leak for 11/14 examples (callback + periodic flush patterns)
- ✅ Reasonable semantics: "pending" means "not yet persisted"
- ✅ Minimal code change
- ✅ No performance impact

**Cons**:
- ❌ Breaks mixed pattern (polling + periodic flush)
- ❌ Semantic change: "pending" changes meaning
- ❌ Users polling between flushes lose data
- ❌ Surprising behavior for polling users

**Affected Use Cases**:
- ✅ Callback-driven: Fixed (no leak)
- ✅ Periodic flush only: Fixed (no leak)
- ❌ Polling: **BROKEN** if flush interval < polling interval

**Example Break Case**:
```cpp
// User wants both polling AND periodic backup
client.set_output_file("backup.csv");
client.set_flush_interval(std::chrono::seconds(30));
client.start(symbols);

while (running) {
    std::this_thread::sleep_for(std::chrono::seconds(60));  // Poll every minute
    auto updates = client.get_updates();  // EMPTY! Cleared by flush at 30s
}
```

**Verdict**: ⚠️ Simple but has edge case issues

---

### Solution 2: Conditional Population (Smart Clear)

**Approach**: Only populate `pending_updates_` if polling pattern is actually being used (no callback set).

**Implementation**:
```cpp
void add_record(const TickerRecord& record) {
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        ticker_history_.push_back(record);

        // Only populate pending if no callback (indicates polling pattern)
        bool should_buffer;
        {
            std::lock_guard<std::mutex> cb_lock(callback_mutex_);
            should_buffer = (update_callback_ == nullptr);
        }

        if (should_buffer) {
            pending_updates_.push_back(record);
        }

        this->check_and_flush();
    }

    // Call user callback (outside data lock)
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (update_callback_) {
            update_callback_(record);
        }
    }
}
```

**Pros**:
- ✅ Elegant: `pending_updates_` only used when needed
- ✅ No leak in callback mode (vector stays empty)
- ✅ Polling pattern works normally
- ✅ Zero memory overhead when using callbacks
- ✅ Semantic clarity: "pending" for polling only

**Cons**:
- ❌ Double locking on hot path (performance concern)
- ❌ Complex logic
- ❌ What if user sets callback after starting?
- ❌ What if user wants both callback AND polling?
- ❌ Race condition window during callback set/unset

**Edge Case - Late Callback Registration**:
```cpp
client.start(symbols);  // No callback → populates pending_updates_

std::this_thread::sleep_for(std::chrono::seconds(5));

client.set_update_callback([](auto& r) { ... });  // Now has callback
// What happens to pending_updates_ accumulated in first 5 seconds?
// It will never be cleared!
```

**Performance Impact**:
- Extra lock acquisition on every record: ~25-50ns
- At 1000 records/sec: 25-50 μs/sec overhead (0.0025-0.005% CPU)
- Negligible for typical use cases

**Verdict**: ⚠️ Clever but has edge cases and complexity

---

### Solution 3: Bounded Queue with Auto-Eviction

**Approach**: Cap `pending_updates_` to prevent unbounded growth, dropping old data when limit reached.

**Implementation**:
```cpp
namespace {
    constexpr size_t MAX_PENDING_UPDATES = 10000;  // ~2MB max memory
}

void add_record(const TickerRecord& record) {
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        ticker_history_.push_back(record);
        pending_updates_.push_back(record);

        // Enforce size limit to prevent memory leak
        if (pending_updates_.size() > MAX_PENDING_UPDATES) {
            // Drop oldest half (cheaper than one-by-one removal)
            pending_updates_.erase(
                pending_updates_.begin(),
                pending_updates_.begin() + MAX_PENDING_UPDATES / 2
            );
        }

        this->check_and_flush();
    }
    // ...
}
```

**Pros**:
- ✅ Bounds memory usage (guaranteed max ~2-10 MB)
- ✅ Doesn't break any existing pattern
- ✅ Defensive programming
- ✅ Simple to understand
- ✅ No semantic changes

**Cons**:
- ❌ Silently drops data (confusing, hard to debug)
- ❌ Arbitrary limit (what's the "right" value?)
- ❌ Doesn't solve the root design issue
- ❌ Performance penalty on every add when at limit
- ❌ Large erase() operation (expensive)

**Data Loss Scenarios**:
```cpp
// Callback user running for hours
client.set_update_callback([](auto& r) { /* process */ });
client.start(symbols);

// After MAX_PENDING_UPDATES records, oldest data silently dropped
// User has no idea this is happening!
```

**Performance Impact**:
- Normal operation: No impact
- At limit: `erase()` of 5000 elements every 5000 records
- Order book: Could hit limit in 10-50 seconds
- **Verdict on perf**: Acceptable but not ideal

**Verdict**: ❌ Band-aid solution, doesn't fix root cause

---

### Solution 4: Usage Pattern Detection (Hybrid)

**Approach**: Auto-detect whether polling is actually being used, clear `pending_updates_` during flush only if not.

**Implementation**:
```cpp
class KrakenWebSocketClientBase ... {
    std::atomic<bool> using_polling_pattern_{false};

public:
    std::vector<TickerRecord> get_updates() {
        std::lock_guard<std::mutex> lock(data_mutex_);
        using_polling_pattern_.store(true, std::memory_order_relaxed);
        std::vector<TickerRecord> updates = std::move(pending_updates_);
        pending_updates_.clear();
        return updates;
    }

private:
    void perform_flush() {
        if (ticker_history_.empty()) {
            return;
        }

        // ... flush logic ...

        // Clear buffers
        ticker_history_.clear();
        ticker_history_.reserve(RECORD_BUFFER_INITIAL_CAPACITY);

        // Only clear pending if not using polling pattern
        if (!using_polling_pattern_.load(std::memory_order_relaxed)) {
            pending_updates_.clear();
        }
    }
};
```

**Pros**:
- ✅ Best of both worlds
- ✅ Polling pattern completely unaffected
- ✅ Callback/flush patterns don't leak
- ✅ Simple atomic flag (no locks)
- ✅ Auto-adapts to actual usage

**Cons**:
- ❌ State tracking complexity
- ❌ One-way flag (once polling detected, never resets)
- ❌ What if user polls once then stops?
- ❌ Non-obvious behavior from user perspective

**Edge Case - Spurious get_updates() Call**:
```cpp
// User primarily uses callbacks
client.set_update_callback([](auto& r) { /* process */ });
client.start(symbols);

// Curiosity: "Let me check pending count"
auto updates = client.get_updates();  // ← Sets using_polling_pattern_ = true!

// Now pending_updates_ will never be auto-cleared → LEAK!
```

**Potential Improvement - Time-Based Detection**:
```cpp
std::atomic<std::chrono::steady_clock::time_point> last_poll_time_;

void perform_flush() {
    // ...

    // Only clear if not polled recently (within 2x flush interval)
    auto now = std::chrono::steady_clock::now();
    auto last_poll = last_poll_time_.load();
    auto time_since_poll = now - last_poll;

    if (time_since_poll > 2 * flush_interval_) {
        pending_updates_.clear();  // Not actively polling
    }
}
```

**Verdict**: ⚠️ Better than Solution 2, but still has edge cases

---

### Solution 5: Separate API for Polling vs Callbacks (Clean Design)

**Approach**: Make the two patterns mutually exclusive with explicit API design.

**Implementation**:
```cpp
class KrakenWebSocketClientBase ... {
    enum class UsageMode {
        UNSET,
        CALLBACK_DRIVEN,
        POLLING
    };

    UsageMode usage_mode_ = UsageMode::UNSET;

public:
    // Explicitly choose polling mode
    void enable_polling_mode() {
        if (usage_mode_ != UsageMode::UNSET) {
            throw std::runtime_error("Usage mode already set");
        }
        usage_mode_ = UsageMode::POLLING;
    }

    void set_update_callback(UpdateCallback callback) {
        if (usage_mode_ == UsageMode::POLLING) {
            throw std::runtime_error("Cannot set callback in polling mode");
        }
        usage_mode_ = UsageMode::CALLBACK_DRIVEN;
        // ... set callback
    }

    std::vector<TickerRecord> get_updates() {
        if (usage_mode_ != UsageMode::POLLING) {
            throw std::runtime_error("Not in polling mode. Call enable_polling_mode() first");
        }
        // ... return updates
    }

private:
    void add_record(const TickerRecord& record) {
        ticker_history_.push_back(record);

        // Only populate pending in polling mode
        if (usage_mode_ == UsageMode::POLLING) {
            pending_updates_.push_back(record);
        }

        this->check_and_flush();

        // Only call callback in callback mode
        if (usage_mode_ == UsageMode::CALLBACK_DRIVEN && update_callback_) {
            update_callback_(record);
        }
    }

    void perform_flush() {
        // ... flush logic ...

        ticker_history_.clear();

        // Clear pending in callback mode (not used anyway)
        if (usage_mode_ == UsageMode::CALLBACK_DRIVEN) {
            pending_updates_.clear();
        }
    }
};
```

**Pros**:
- ✅ Clean, explicit API design
- ✅ No ambiguity about usage pattern
- ✅ No memory leak possible
- ✅ Clear error messages for misuse
- ✅ Optimal performance (no unnecessary buffering)
- ✅ Future-proof design

**Cons**:
- ❌ **Breaking API change** (major version bump)
- ❌ Requires updating all existing code
- ❌ More restrictive (no hybrid patterns)
- ❌ Users must choose upfront

**Migration Path**:
```cpp
// OLD CODE
client.set_update_callback([](auto& r) { ... });
client.start(symbols);

// NEW CODE
client.enable_polling_mode();  // ERROR! Can't use both
// OR
client.set_update_callback([](auto& r) { ... });  // Implicitly callback mode
client.start(symbols);
```

**Verdict**: ✅ Best long-term solution but requires major version bump

---

### Solution 6: Clear During Flush + Document Limitation (Pragmatic)

**Approach**: Implement Solution 1 + thorough documentation of limitations.

**Implementation**:
```cpp
void perform_flush() {
    // ... existing flush logic ...

    // Clear buffers
    ticker_history_.clear();
    ticker_history_.reserve(RECORD_BUFFER_INITIAL_CAPACITY);

    // Also clear pending_updates_ to prevent memory leak in callback mode
    // NOTE: If using polling pattern, call get_updates() more frequently
    //       than flush_interval_ to avoid data loss
    pending_updates_.clear();
}

/**
 * Get pending updates (polling pattern)
 *
 * WARNING: In polling mode, you must call this more frequently than the
 * configured flush_interval_ (default 30s). During periodic flush,
 * pending_updates_ is cleared to prevent memory leaks in callback mode.
 *
 * Recommended: Call get_updates() at least every flush_interval_ / 2
 *
 * If you need both polling AND periodic flushing, consider calling
 * get_updates() immediately before calling flush().
 */
std::vector<TickerRecord> get_updates();
```

**Pros**:
- ✅ Simple implementation
- ✅ Fixes leak for majority use cases (11/14 examples)
- ✅ Clear documentation of limitation
- ✅ No API breaking changes
- ✅ Minimal code changes

**Cons**:
- ❌ Still has edge case (polling slower than flush)
- ❌ Requires users to read documentation
- ❌ Subtle behavior that could surprise users

**Best Practices Documentation**:
```markdown
## Usage Patterns

### Callback-Driven (Recommended for most use cases)
- No need to call get_updates()
- Data delivered via callback immediately
- No memory leak (pending_updates_ auto-cleared)

### Polling
- Call get_updates() regularly (recommended: every 1-5 seconds)
- Must poll faster than flush_interval_ to avoid data loss
- Example: If flush_interval_ = 30s, poll every 10-15s

### Mixed Pattern (Advanced)
If you need both polling AND periodic flushing:
1. Set flush_interval_ > polling_interval_
2. OR call get_updates() before flush()
```

**Verdict**: ✅ Best pragmatic solution for current version

---

## Comparison Matrix

| Solution | Complexity | API Breaking | Memory Leak Fixed | Polling Works | Performance | Maintainability |
|----------|-----------|--------------|-------------------|---------------|-------------|-----------------|
| 1. Clear on flush | Low | No | ✅ | ⚠️ (with caveats) | ✅ Excellent | ✅ High |
| 2. Conditional populate | Medium | No | ✅ | ✅ | ⚠️ (double lock) | ⚠️ Medium |
| 3. Bounded queue | Low | No | ⚠️ (limited) | ✅ | ⚠️ (erase cost) | ⚠️ Low (band-aid) |
| 4. Usage detection | Medium | No | ✅ | ✅ | ✅ Excellent | ⚠️ Medium |
| 5. Explicit modes | High | ⚠️ **YES** | ✅ | ✅ | ✅ Excellent | ✅ High |
| 6. Clear + docs | Low | No | ✅ | ⚠️ (documented) | ✅ Excellent | ✅ High |

---

## Recommendation

### For Current Version (v1.x)

**Implement Solution 6**: Clear during flush + thorough documentation

**Rationale**:
1. Fixes leak for 11/14 examples immediately
2. No API breaking changes
3. Simple, maintainable code
4. Clear documentation helps users avoid edge cases
5. Polling users can easily adapt (poll more frequently)

**Implementation Priority**: HIGH (should be in next bug-fix release)

---

### For Next Major Version (v2.0)

**Implement Solution 5**: Explicit mode selection

**Rationale**:
1. Clean, unambiguous API design
2. Eliminates all edge cases
3. Better performance (no unnecessary buffering)
4. Sets foundation for future enhancements
5. Major version allows breaking changes

**Example API**:
```cpp
// Polling mode
client.enable_polling_mode();
auto updates = client.get_updates();

// Callback mode (default)
client.set_update_callback([](auto& r) { ... });

// Attempting both
client.enable_polling_mode();
client.set_update_callback([](auto& r) { ... });  // Throws exception
```

---

## Testing Strategy

### Unit Tests Needed

1. **Memory Leak Test - Callback Mode**:
```cpp
TEST(MemoryLeakTest, CallbackModeDoesNotLeak) {
    KrakenWebSocketClient client;
    client.set_update_callback([](auto& r) { });
    client.start({"BTC/USD"});

    // Simulate 1 hour of updates
    for (int i = 0; i < 36000; i++) {
        // Wait for update
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // pending_updates_ should be cleared by periodic flush
    EXPECT_LT(client.pending_count(), 100);  // Should be near zero
}
```

2. **Polling Pattern Test**:
```cpp
TEST(PollingTest, RegularPollingWorks) {
    KrakenWebSocketClient client;
    client.start({"BTC/USD"});

    int total_updates = 0;
    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto updates = client.get_updates();
        total_updates += updates.size();
    }

    EXPECT_GT(total_updates, 0);
}
```

3. **Edge Case Test - Polling Slower Than Flush**:
```cpp
TEST(EdgeCaseTest, SlowPollingLosesData) {
    KrakenWebSocketClient client;
    client.set_flush_interval(std::chrono::seconds(5));
    client.start({"BTC/USD"});

    std::this_thread::sleep_for(std::chrono::seconds(10));  // Wait past flush

    auto updates = client.get_updates();
    // May be empty if flushed between polls (documented behavior)
}
```

### Integration Tests

1. **Long-Running Memory Test**:
   - Run callback mode for 1 hour
   - Monitor RSS memory
   - Verify no unbounded growth

2. **High-Frequency Test**:
   - Subscribe to order book (100+ updates/sec)
   - Run for 10 minutes
   - Verify memory stable

---

## Documentation Updates Needed

### 1. API Documentation

**get_updates() method**:
```cpp
/**
 * Get pending updates (polling pattern)
 *
 * Returns all updates received since the last call to get_updates().
 * This is the primary method for the polling usage pattern.
 *
 * IMPORTANT: If using periodic flushing (set_output_file + set_flush_interval),
 * you must call get_updates() more frequently than the flush_interval_ to avoid
 * data loss. Recommended: poll at least every (flush_interval_ / 2).
 *
 * During periodic flush, pending_updates_ is automatically cleared to prevent
 * memory leaks in callback-driven mode.
 *
 * @return Vector of all pending ticker records (moves data, clears internal buffer)
 *
 * @example Correct polling pattern:
 *   client.set_flush_interval(std::chrono::seconds(30));
 *   while (running) {
 *     std::this_thread::sleep_for(std::chrono::seconds(10));  // < 30s
 *     auto updates = client.get_updates();
 *     // process updates
 *   }
 */
std::vector<TickerRecord> get_updates();
```

### 2. Usage Guide

Add to README or user guide:

```markdown
## Usage Patterns

### Callback-Driven Mode (Recommended)

Best for:
- Real-time processing
- Event-driven architectures
- When you need immediate notification

No need to call get_updates(). Data is delivered via callback.

### Polling Mode

Best for:
- Batch processing
- When you control the processing loop
- Integration with existing polling architecture

Must call get_updates() regularly to retrieve data.

⚠️ **Important**: If using periodic flushing with polling, call get_updates()
more frequently than your flush interval to avoid data loss.

### Not Recommended: Mixed Polling + Slow Flush

❌ Don't do this:
```cpp
client.set_flush_interval(std::chrono::seconds(30));
while (running) {
    std::this_thread::sleep_for(std::chrono::seconds(60));  // TOO SLOW!
    auto updates = client.get_updates();  // May be empty (cleared by flush)
}
```

✅ Do this instead:
```cpp
client.set_flush_interval(std::chrono::seconds(30));
while (running) {
    std::this_thread::sleep_for(std::chrono::seconds(10));  // Fast enough
    auto updates = client.get_updates();
}
```
```

### 3. Migration Guide (for v2.0 if implementing Solution 5)

```markdown
## v2.0 Breaking Changes

### Explicit Mode Selection Required

v1.x allowed implicit mode switching. v2.0 requires explicit mode selection.

**Before (v1.x)**:
```cpp
client.set_update_callback([](auto& r) { ... });
client.start(symbols);
```

**After (v2.0)**:
```cpp
// Callback mode (default if you set a callback)
client.set_update_callback([](auto& r) { ... });
client.start(symbols);

// OR explicitly enable polling mode
client.enable_polling_mode();
client.start(symbols);
auto updates = client.get_updates();
```

Attempting to use both modes will throw an exception.
```

---

## Implementation Checklist

### Phase 1: Solution 6 (Current Version)

- [ ] Modify `perform_flush()` to clear `pending_updates_`
- [ ] Add warning comment in code
- [ ] Update `get_updates()` documentation
- [ ] Add usage pattern guide to README
- [ ] Write unit tests for memory leak fix
- [ ] Write integration tests for edge cases
- [ ] Update all example comments if needed
- [ ] Test with all 14 examples
- [ ] Measure memory usage before/after
- [ ] Create pull request with detailed description

### Phase 2: Solution 5 (v2.0)

- [ ] Design explicit mode API
- [ ] Implement UsageMode enum and checks
- [ ] Add enable_polling_mode() method
- [ ] Update set_update_callback() to set mode
- [ ] Update get_updates() to check mode
- [ ] Update add_record() to respect mode
- [ ] Update perform_flush() to respect mode
- [ ] Write migration guide
- [ ] Update all examples for v2.0 API
- [ ] Update documentation
- [ ] Deprecate v1.x API
- [ ] Release notes for breaking changes

---

## Related Issues

This issue is related to:
- Original code review (docs/code_review_websocket_client_base.md)
- API clarity improvements (docs/api_improvement_flush_vs_save.md)
- Memory management in long-running sessions

---

## Conclusion

The `pending_updates_` memory leak is a **high-priority issue** affecting most production use cases. Solution 6 (clear during flush + documentation) provides the best balance of:
- Immediate fix for leak
- No API breaking changes
- Simple, maintainable implementation
- Clear path to v2.0 with cleaner API (Solution 5)

**Recommended Action**: Implement Solution 6 in next patch release (v1.x.y), plan Solution 5 for v2.0.
