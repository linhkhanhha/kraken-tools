# API Improvement: flush() vs save_to_csv()

**Date**: 2025-11-12
**Issue**: #3 - Confusing save_to_csv() API Behavior
**Status**: ✅ FIXED

---

## Problem

The original `save_to_csv()` method had confusing behavior:

```cpp
// BEFORE: Confusing behavior
void save_to_csv(const std::string& filename) {
    if (!output_filename_.empty() && csv_header_written_) {
        // Ignores 'filename' parameter!
        this->force_flush();
    } else {
        // Uses 'filename' parameter
        Utils::save_to_csv(filename, ticker_history_);
    }
}
```

**Issues**:
1. Parameter `filename` sometimes ignored (when periodic flushing is active)
2. Parameter `filename` sometimes used (when no periodic flushing)
3. No way for caller to predict behavior without checking internal state
4. Method name doesn't indicate dual behavior

---

## Solution

Split functionality into two distinct methods:

### 1. `flush()` - For Periodic Flushing Use Case

```cpp
/**
 * Flush remaining buffered data to configured output file
 * Use this after calling set_output_file() to ensure all data is written
 * NOTE: Does nothing if no output file is configured
 */
void flush();
```

**When to use**:
- After calling `set_output_file()` to configure periodic flushing
- At shutdown to flush remaining buffered data
- When you want data written to the already-configured file

**Example**:
```cpp
KrakenWebSocketClientSimdjsonV2 client;

// Configure periodic flushing
client.set_output_file("ticker_data.csv");
client.set_flush_interval(std::chrono::seconds(30));
client.start({"BTC/USD", "ETH/USD"});

// ... run for a while ...

// At shutdown, flush remaining data
client.flush();  // Writes to ticker_data.csv
client.stop();
```

---

### 2. `save_to_csv()` - For One-Shot Snapshot Use Case

```cpp
/**
 * Save all historical data to a specific file (one-shot snapshot)
 * This creates a new file with all data and header, regardless of set_output_file()
 * Use this for ad-hoc exports or when not using periodic flushing
 * @param filename Target file to write snapshot
 */
void save_to_csv(const std::string& filename);
```

**When to use**:
- Creating ad-hoc exports/snapshots
- NOT using periodic flushing (no `set_output_file()` call)
- Want to save to a specific file regardless of configuration

**Example**:
```cpp
KrakenWebSocketClientSimdjsonV2 client;

// No set_output_file() - just accumulate data in memory
client.start({"BTC/USD", "ETH/USD"});

// ... run for a while ...

// At shutdown, save snapshot
client.save_to_csv("my_snapshot.csv");  // Always writes to this file
client.stop();
```

---

## Behavior Comparison

| Scenario | Old API | New API |
|----------|---------|---------|
| Periodic flushing active | `save_to_csv(file)` ignores parameter | `flush()` writes to configured file |
| No periodic flushing | `save_to_csv(file)` uses parameter | `save_to_csv(file)` uses parameter |
| Ad-hoc export | Not possible | `save_to_csv(file)` always works |

---

## Migration Guide

### Case 1: Using Periodic Flushing

**Before**:
```cpp
client.set_output_file("data.csv");
client.start(symbols);
// ... run ...
client.save_to_csv("data.csv");  // Parameter was ignored anyway!
```

**After**:
```cpp
client.set_output_file("data.csv");
client.start(symbols);
// ... run ...
client.flush();  // Explicit and clear
```

---

### Case 2: One-Shot Snapshot (No Change Needed)

**Before & After** (same):
```cpp
client.start(symbols);
// ... run ...
client.save_to_csv("snapshot.csv");  // Works the same
```

---

### Case 3: Multiple Exports (Now Possible!)

**After** (new capability):
```cpp
client.set_output_file("continuous.csv");
client.start(symbols);
// ... run for a while ...

// Export snapshot for analysis while continuing
client.save_to_csv("hourly_snapshot.csv");

// ... continue running ...

// At shutdown, flush remaining data
client.flush();
```

---

## Implementation Details

### flush() Implementation

```cpp
void flush() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    if (output_filename_.empty()) {
        std::cerr << "[Warning] flush() called but no output file configured." << std::endl;
        return;
    }

    if (ticker_history_.empty()) {
        return;  // Nothing to flush
    }

    // Force flush remaining buffered data to configured file
    this->force_flush();
    std::cout << "\nFinal flush completed." << std::endl;
}
```

**Key points**:
- Checks if output file is configured
- Uses internal `force_flush()` from FlushSegmentMixin
- Writes to `output_file_` (persistent file handle)

---

### save_to_csv() Implementation

```cpp
void save_to_csv(const std::string& filename) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    // Always create a one-shot snapshot to the specified file
    // This is independent of the configured output_file_ for periodic flushing
    Utils::save_to_csv(filename, ticker_history_);
}
```

**Key points**:
- Always honors the `filename` parameter
- Creates completely new file with header
- Independent of periodic flushing configuration
- Uses `Utils::save_to_csv()` (one-shot write)

---

## Affected Files

### Code Changes
- ✅ `lib/kraken_websocket_client_base.hpp` - Added `flush()`, updated `save_to_csv()` (lines 73-346)

### Example Updates
- ✅ `examples/retrieve_kraken_live_data_level1.cpp` - Changed to use `flush()` (line 317)
- ℹ️ Other examples - No change needed (already using correctly)

### Documentation
- ✅ `docs/code_review_fixes_summary.md` - Updated with fix details
- ✅ `docs/api_improvement_flush_vs_save.md` - This document

---

## Benefits

1. **Predictable Behavior**: Each method has clear, consistent behavior
2. **Clear Intent**: Method names reflect their purpose
3. **No Hidden Surprises**: Parameters are always honored
4. **Better Separation**: Periodic flushing vs one-shot exports are distinct operations
5. **More Flexible**: Can now do both periodic flushing AND ad-hoc exports

---

## Backward Compatibility

**Mostly Compatible**: Existing code continues to work but may produce warnings:

```cpp
// Old code that used periodic flushing
client.set_output_file("data.csv");
client.start(symbols);
client.save_to_csv("data.csv");  // Still works, but creates duplicate snapshot
```

**Recommended**: Update to use `flush()` for clearer intent and avoid duplication.

---

## Testing

### Unit Test Cases

1. **flush() with no output file configured**:
   ```cpp
   client.flush();  // Should print warning
   ```

2. **flush() with output file configured**:
   ```cpp
   client.set_output_file("test.csv");
   client.start(symbols);
   client.flush();  // Should write to test.csv
   ```

3. **save_to_csv() always creates snapshot**:
   ```cpp
   client.set_output_file("periodic.csv");
   client.start(symbols);
   client.save_to_csv("snapshot.csv");  // Should write to snapshot.csv
   ```

4. **Both methods work together**:
   ```cpp
   client.set_output_file("periodic.csv");
   client.start(symbols);
   client.save_to_csv("backup.csv");   // Creates backup
   client.flush();                      // Flushes to periodic.csv
   ```

---

## Future Considerations

### Potential Enhancements

1. **Return value for save_to_csv()**:
   ```cpp
   bool save_to_csv(const std::string& filename);  // Returns success/failure
   ```

2. **Flush with custom filename**:
   ```cpp
   void flush(const std::string& override_filename = "");
   ```

3. **Clear history after save**:
   ```cpp
   void save_to_csv_and_clear(const std::string& filename);
   ```

These are NOT implemented yet but could be considered for future versions.

---

## Conclusion

The API is now clearer and more predictable:
- Use `flush()` when you're using periodic flushing with `set_output_file()`
- Use `save_to_csv()` for one-shot snapshots to specific files
- Both can be used together for maximum flexibility

This improves code maintainability and reduces confusion for users of the API.
