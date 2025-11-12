# Code Redundancy Analysis and Refactoring Proposal

**Date**: 2025-11-12
**Analyzed by**: Claude Code

---

## Executive Summary

Analysis of the C++ codebase reveals **significant code duplication** in the recently-added memory management and file segmentation features. The same flushing and segmentation logic is implemented independently in:

1. **`kraken_websocket_client_base.hpp`** (Level 1 ticker data, CSV format)
2. **`jsonl_writer.hpp/cpp`** (Level 2 order book data, JSONL format)

This duplication creates **maintenance risks**: bug fixes must be applied in multiple places, and future enhancements (e.g., for Level 3) would require copy-pasting the same code again.

**Recommendation**: Extract common flush/segment logic into a reusable **`FlushSegmentManager`** base class or mixin.

---

## Identified Redundancies

### 1. **SegmentMode Enum (Duplicated)**

**Location 1**: `cpp/lib/kraken_websocket_client_base.hpp:19`
```cpp
enum class SegmentMode {
    NONE,    // Single file (default)
    HOURLY,  // One file per hour
    DAILY    // One file per day
};
```

**Location 2**: `cpp/lib/jsonl_writer.hpp:26`
```cpp
enum class SegmentMode {
    NONE,    // Single file (default)
    HOURLY,  // One file per hour (YYYYMMDD_HH)
    DAILY    // One file per day (YYYYMMDD)
};
```

**Impact**: Identical definition in two places. If we add new modes (e.g., `WEEKLY`, `MONTHLY`), both must be updated.

---

### 2. **Flush Configuration (Duplicated)**

**Level 1** (`kraken_websocket_client_base.hpp`):
```cpp
std::chrono::seconds flush_interval_;         // Line 96
size_t memory_threshold_bytes_;               // Line 97

// Constructor
flush_interval_(30),                          // Line 146
memory_threshold_bytes_(10 * 1024 * 1024),   // Line 147

// Setters
void set_flush_interval(std::chrono::seconds interval);  // Line 64
void set_memory_threshold(size_t bytes);                 // Line 65
```

**Level 2** (`jsonl_writer.hpp/cpp`):
```cpp
std::chrono::seconds flush_interval_;         // Line 125
size_t memory_threshold_bytes_;               // Line 126

// Constructor
flush_interval_(30),                          // Line 17
memory_threshold_bytes_(10 * 1024 * 1024),   // Line 18

// Setters
void set_flush_interval(std::chrono::seconds interval);  // Line 78
void set_memory_threshold(size_t bytes);                 // Line 84
```

**Impact**: Same configuration pattern, same defaults, same setter interface.

---

### 3. **should_flush() Logic (Duplicated)**

**Level 1** (`kraken_websocket_client_base.hpp:498-529`):
```cpp
bool KrakenWebSocketClientBase<JsonParser>::should_flush() const {
    if (output_filename_.empty() || ticker_history_.empty()) {
        return false;
    }

    bool time_exceeded = false;
    if (flush_interval_.count() > 0) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_flush_time_);
        time_exceeded = (elapsed >= flush_interval_);
    }

    bool memory_exceeded = false;
    if (memory_threshold_bytes_ > 0) {
        size_t memory_bytes = ticker_history_.size() * sizeof(TickerRecord);
        memory_exceeded = (memory_bytes >= memory_threshold_bytes_);
    }

    return time_exceeded || memory_exceeded;  // OR logic
}
```

**Level 2** (`jsonl_writer.cpp:215-238`):
```cpp
bool JsonLinesWriter::should_flush() const {
    if (record_buffer_.empty()) {
        return false;
    }

    bool time_exceeded = false;
    if (flush_interval_.count() > 0) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_flush_time_);
        time_exceeded = (elapsed >= flush_interval_);
    }

    bool memory_exceeded = false;
    if (memory_threshold_bytes_ > 0) {
        size_t memory_bytes = record_buffer_.size() * sizeof(OrderBookRecord);
        memory_exceeded = (memory_bytes >= memory_threshold_bytes_);
    }

    return time_exceeded || memory_exceeded;  // OR logic
}
```

**Differences**:
- Container name (`ticker_history_` vs `record_buffer_`)
- Record type (`TickerRecord` vs `OrderBookRecord`)
- **Logic is 99% identical**

**Impact**: Bug fixes to flushing logic must be replicated manually.

---

### 4. **generate_segment_key() Logic (Duplicated)**

**Level 1** (`kraken_websocket_client_base.hpp:532-557`):
```cpp
std::string generate_segment_key() const {
    if (segment_mode_ == SegmentMode::NONE) {
        return "";
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&time_t);

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (tm.tm_year + 1900)
        << std::setw(2) << (tm.tm_mon + 1)
        << std::setw(2) << tm.tm_mday;

    if (segment_mode_ == SegmentMode::HOURLY) {
        oss << "_" << std::setw(2) << tm.tm_hour;
    }

    return oss.str();
}
```

**Level 2** (`jsonl_writer.cpp:269-285`):
```cpp
std::string generate_segment_key() const {
    auto now = std::time(nullptr);
    auto tm = *std::gmtime(&now);

    char buffer[32];
    if (segment_mode_ == SegmentMode::HOURLY) {
        std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H", &tm);
    } else if (segment_mode_ == SegmentMode::DAILY) {
        std::strftime(buffer, sizeof(buffer), "%Y%m%d", &tm);
    } else {
        return "";
    }

    return std::string(buffer);
}
```

**Differences**:
- Level 1 uses `std::ostringstream` + `std::setw()`
- Level 2 uses `std::strftime()`
- **Functionally identical output**

**Impact**: Inconsistent implementation for same task. Level 2's `strftime()` is simpler.

---

### 5. **insert_segment_key() / generate_segment_filename() (Duplicated)**

**Level 1** (`kraken_websocket_client_base.hpp:560-587`):
```cpp
std::string generate_segment_filename() const {
    if (segment_mode_ == SegmentMode::NONE || base_output_filename_.empty()) {
        return base_output_filename_;
    }

    std::string segment_key = generate_segment_key();
    if (segment_key.empty()) {
        return base_output_filename_;
    }

    size_t dot_pos = base_output_filename_.find_last_of('.');

    if (dot_pos != std::string::npos) {
        // "output.csv" -> "output.20251112_10.csv"
        return base_output_filename_.substr(0, dot_pos) + "." +
               segment_key +
               base_output_filename_.substr(dot_pos);
    } else {
        // "output" -> "output.20251112_10"
        return base_output_filename_ + "." + segment_key;
    }
}
```

**Level 2** (`jsonl_writer.cpp:323-335`):
```cpp
std::string insert_segment_key(const std::string& base, const std::string& key) const {
    size_t ext_pos = base.rfind(".jsonl");
    if (ext_pos == std::string::npos) {
        return base + "." + key + ".jsonl";
    }

    std::string result = base.substr(0, ext_pos);
    result += "." + key + ".jsonl";
    return result;
}
```

**Differences**:
- Level 1 is more generic (works with any extension)
- Level 2 is JSONL-specific
- **Core logic identical**: insert segment key before extension

**Impact**: Level 1's implementation is superior (generic). Level 2's could be replaced.

---

### 6. **Segment Transition Logic (Duplicated)**

Both implementations have similar logic for detecting segment transitions:

**Level 1**:
```cpp
if (should_transition_segment()) {
    transition_segment();
}
```

**Level 2**:
```cpp
if (should_transition_segment()) {
    if (!record_buffer_.empty()) {
        flush_buffer();
    }
    create_new_segment();
}
```

**Impact**: Same pattern, minor variations in implementation.

---

### 7. **Statistics Tracking (Duplicated)**

**Level 1**:
```cpp
size_t flush_count_;
size_t segment_count_;
std::string current_segment_key_;
std::string current_segment_filename_;
```

**Level 2**:
```cpp
size_t flush_count_;
size_t segment_count_;
std::string current_segment_key_;
std::string current_segment_filename_;
```

**Impact**: Identical variable names, identical purpose.

---

## Impact Assessment

### Current State

| Feature | Level 1 (CSV) | Level 2 (JSONL) | Level 3 (CSV/JSONL) |
|---------|---------------|-----------------|---------------------|
| Periodic Flush | ✅ Implemented | ✅ Implemented | ❌ Not Implemented |
| Memory Threshold | ✅ Implemented | ✅ Implemented | ❌ Not Implemented |
| Segmentation | ✅ Implemented | ✅ Implemented | ❌ Not Implemented |
| Code Sharing | ❌ Duplicated | ❌ Duplicated | N/A |

### Maintenance Burden

**Example Scenario**: Bug found in `should_flush()` OR logic

**Current Approach**:
1. Fix bug in `kraken_websocket_client_base.hpp`
2. **Remember** to fix the same bug in `jsonl_writer.cpp`
3. **Remember** to add same logic to Level 3 if needed

**Risk**: Forgetting to apply fix everywhere leads to inconsistent behavior.

---

## Proposed Solution

### Option 1: **Extract FlushSegmentManager Base Class** ⭐ (Recommended)

Create a reusable base class for flush/segment logic:

```cpp
// cpp/lib/flush_segment_manager.hpp

namespace kraken {

enum class SegmentMode {
    NONE,    // Single file (default)
    HOURLY,  // One file per hour (YYYYMMDD_HH)
    DAILY    // One file per day (YYYYMMDD)
};

class FlushSegmentManager {
protected:
    // Configuration
    std::chrono::seconds flush_interval_;
    size_t memory_threshold_bytes_;
    SegmentMode segment_mode_;

    // State
    std::chrono::steady_clock::time_point last_flush_time_;
    size_t flush_count_;
    std::string base_filename_;
    std::string current_segment_key_;
    std::string current_segment_filename_;
    size_t segment_count_;

public:
    FlushSegmentManager();

    // Configuration
    void set_flush_interval(std::chrono::seconds interval);
    void set_memory_threshold(size_t bytes);
    void set_segment_mode(SegmentMode mode);

    // Getters
    size_t get_flush_count() const;
    size_t get_segment_count() const;
    std::string get_current_segment_filename() const;

protected:
    // Core logic (to be called by derived classes)
    bool should_flush(size_t buffer_size, size_t record_size) const;
    bool should_transition_segment() const;
    std::string generate_segment_key() const;
    std::string generate_segment_filename(const std::string& base) const;
    void record_flush();
    void record_segment_transition();
};

} // namespace kraken
```

**Usage in Level 1**:
```cpp
template<typename JsonParser>
class KrakenWebSocketClientBase : public FlushSegmentManager {
    // Remove duplicate members
    // Use inherited methods

    bool should_flush() const override {
        return FlushSegmentManager::should_flush(
            ticker_history_.size(),
            sizeof(TickerRecord)
        );
    }
};
```

**Usage in Level 2**:
```cpp
class JsonLinesWriter : public FlushSegmentManager {
    // Remove duplicate members
    // Use inherited methods

    bool should_flush() const override {
        return FlushSegmentManager::should_flush(
            record_buffer_.size(),
            sizeof(OrderBookRecord)
        );
    }
};
```

**Benefits**:
✅ Single source of truth for flush/segment logic
✅ Bug fixes applied once, affect all users
✅ Easy to add Level 3 support
✅ Consistent behavior across all levels
✅ Testable in isolation

**Drawbacks**:
❌ Requires refactoring existing code
❌ Adds inheritance layer

---

### Option 2: **Extract Free Functions/Namespace**

Create utility functions in a shared namespace:

```cpp
// cpp/lib/flush_segment_utils.hpp

namespace kraken::flush_segment {

enum class SegmentMode {
    NONE, HOURLY, DAILY
};

bool should_flush(
    std::chrono::seconds flush_interval,
    size_t memory_threshold,
    std::chrono::steady_clock::time_point last_flush,
    size_t current_memory
);

std::string generate_segment_key(SegmentMode mode);

std::string insert_segment_key(
    const std::string& base_filename,
    const std::string& segment_key,
    const std::string& extension
);

} // namespace kraken::flush_segment
```

**Benefits**:
✅ No inheritance required
✅ Simple to adopt incrementally
✅ Pure functions (easier to test)

**Drawbacks**:
❌ Still requires passing many parameters
❌ State management left to caller
❌ Less encapsulation

---

### Option 3: **Template Mixin (CRTP)**

Use Curiously Recurring Template Pattern:

```cpp
template<typename Derived>
class FlushSegmentMixin {
protected:
    // ... flush/segment logic ...

    bool should_flush() const {
        auto* derived = static_cast<const Derived*>(this);
        size_t memory = derived->get_buffer_size() * derived->get_record_size();
        // ... rest of logic ...
    }
};

class JsonLinesWriter : public FlushSegmentMixin<JsonLinesWriter> {
    size_t get_buffer_size() const { return record_buffer_.size(); }
    size_t get_record_size() const { return sizeof(OrderBookRecord); }
};
```

**Benefits**:
✅ No virtual function overhead
✅ Type-safe at compile time
✅ Zero runtime cost

**Drawbacks**:
❌ More complex template code
❌ Harder to understand for maintainers

---

## Recommendation: Option 1 (Base Class)

**Why Option 1**:

1. **Simplicity**: Clear inheritance hierarchy, easy to understand
2. **Consistency**: All writers inherit same behavior
3. **Testability**: Can test `FlushSegmentManager` independently
4. **Extensibility**: Easy to add new features (e.g., compression, async I/O)
5. **Level 3 Ready**: Can add to Level 3 writers immediately

**Implementation Plan**:

### Phase 1: Extract Base Class
1. Create `cpp/lib/flush_segment_manager.hpp`
2. Create `cpp/lib/flush_segment_manager.cpp`
3. Move common logic from Level 1 and Level 2
4. Add unit tests for base class

### Phase 2: Refactor Level 1
1. Modify `kraken_websocket_client_base.hpp` to inherit from `FlushSegmentManager`
2. Remove duplicate members
3. Delegate to base class methods
4. Test Level 1 tool (ensure backward compatibility)

### Phase 3: Refactor Level 2
1. Modify `jsonl_writer.hpp` to inherit from `FlushSegmentManager`
2. Remove duplicate members
3. Delegate to base class methods
4. Test Level 2 tool (ensure backward compatibility)

### Phase 4: Add to Level 3 (Optional)
1. Modify `level3_csv_writer` to inherit from `FlushSegmentManager`
2. Modify `level3_jsonl_writer` to inherit from `FlushSegmentManager`
3. Add CLI arguments to `retrieve_kraken_live_data_level3`

---

## Additional Redundancies

### Minor Redundancies

1. **CLI Argument Parsing**: `-f`, `-m`, `--hourly`, `--daily` arguments duplicated across:
   - `retrieve_kraken_live_data_level1.cpp`
   - `retrieve_kraken_live_data_level2.cpp`
   - Could be extracted to `cli_utils` as a helper function

2. **Configuration Display**: Printing flush/segment config duplicated
   - Could be a method on `FlushSegmentManager`

3. **Statistics Output**: Shutdown summary includes flush/segment stats
   - Could be a formatted string from `FlushSegmentManager`

---

## Estimated Impact

### Lines of Code Reduction

| Component | Current | After Refactor | Reduction |
|-----------|---------|----------------|-----------|
| Level 1 | ~600 lines | ~450 lines | -150 lines |
| Level 2 | ~500 lines | ~350 lines | -150 lines |
| Base Class | 0 lines | ~300 lines | +300 lines |
| **Total** | **1100 lines** | **1100 lines** | **0 lines** |

**Note**: No net reduction, but **much better organization**.

### Maintenance Burden Reduction

| Task | Current | After Refactor |
|------|---------|----------------|
| Fix flush bug | 2 files | 1 file |
| Add new segment mode | 2 files | 1 file |
| Add Level 3 support | Copy-paste | Inherit |
| Test flush logic | 2 test suites | 1 test suite |

---

## Risk Assessment

### Refactoring Risks

| Risk | Severity | Mitigation |
|------|----------|------------|
| Breaking existing code | High | Comprehensive testing before/after |
| Performance regression | Low | No runtime overhead expected |
| Merge conflicts | Medium | Do in dedicated branch |
| Incomplete migration | Medium | Do all at once or flag as deprecated |

### Mitigation Strategies

1. **Preserve Backward Compatibility**: Keep old API surface, delegate internally
2. **Comprehensive Testing**: Run all existing tests + new unit tests
3. **Staged Rollout**: Refactor Level 1, test, then Level 2, then Level 3
4. **Code Review**: Careful review of all changes

---

## Conclusion

The current codebase has **significant duplication** in flush/segment logic between Level 1 and Level 2. This creates **maintenance burden** and **inconsistency risk**.

**Recommended Action**: Extract common logic into `FlushSegmentManager` base class.

**Priority**: **Medium-High**
- Not urgent (current code works)
- But important before adding more features
- Prevents accumulating technical debt

**Next Steps**:
1. Review this analysis
2. Approve approach (Option 1, 2, or 3)
3. Create implementation plan with milestones
4. Begin refactoring in feature branch
5. Comprehensive testing before merge

---

**End of Analysis**
