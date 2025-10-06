# Level 2 Order Book Tool - Design Summary

## What We Documented

Complete design document for the Level 2 order book data capture tool based on collaborative brainstorming session.

**Document Location:** `cpp/docs/LEVEL2_ORDERBOOK_DESIGN.md`

## Design Decisions Summary

### ✅ Decision 1: Order Book Depth
- **Configurable:** Yes via `-d` flag
- **Default:** 10
- **Options:** 10, 25, 100, 500, 1000

### ✅ Decision 2: Data Capture Strategy
- **Approach:** Save raw snapshots + updates (Option A)
- **Architecture:** Two-tool design
  - Tool 1: `retrieve_kraken_live_data_level2` (capture)
  - Tool 2: `process_orderbook_snapshots` (processing - future)
- **Rationale:** Separation of concerns, preserve raw data for flexibility

### ✅ Decision 3: Output Format
- **Format:** JSON Lines (.jsonl)
- **Rationale:** Streamable, appendable, industry standard

### ✅ Decision 4: Checksum Validation
- **Default:** Validate with warnings (Option B)
- **Optional:** `--skip-validation` flag for speed (Option A)
- **Rationale:** Detect corruption but don't stop capture

### ✅ Decision 5: Display Options
- **Approach:** Tiered flags - only compute what's requested
- **Flags:**
  - *(default)* - Minimal counters (fastest)
  - `-v` / `--show-updates` - Update details
  - `--show-top` - Top-of-book display
  - `--show-book` - Full order book (single pair only)
- **Rationale:** Performance - no unnecessary computation

### ✅ Decision 6: Input Methods
- **Same as Level 1:** Yes
- **Formats:** Direct list, text file, CSV file
- **Uses:** cli_utils library

### ✅ Decision 7: Additional Options
- **Default output:** `kraken_orderbook.jsonl`
- **Snapshot param:** Always true (not configurable)
- **Symbol mapping:** Automatic (same as Level 1)
- **Multiple files:** `--separate-files` flag, default single file

## Tool Specification

### Command-Line Interface
```bash
./retrieve_kraken_live_data_level2 [OPTIONS]

Required:
  -p, --pairs <SPEC>          Pairs specification

Optional:
  -d, --depth <NUM>           Depth (10, 25, 100, 500, 1000) [default: 10]
  -o, --output <FILE>         Output file [default: kraken_orderbook.jsonl]
  --separate-files            Create separate file per symbol
  --skip-validation           Skip checksum validation

Display:
  -v, --show-updates          Show update details
  --show-top                  Show top-of-book
  --show-book                 Show full order book (single pair)
```

### Usage Examples
```bash
# Minimal (fastest)
./retrieve_kraken_live_data_level2 -p "BTC/USD,ETH/USD"

# With depth and display
./retrieve_kraken_live_data_level2 -p "BTC/USD" -d 25 --show-top

# From file, separate outputs
./retrieve_kraken_live_data_level2 -p tickers.txt:10 --separate-files

# Full monitoring
./retrieve_kraken_live_data_level2 -p "BTC/USD" --show-book -v --show-top
```

## Design Principles

1. **Separation of Concerns**
   - Capture tool: Fast, lightweight, saves raw data
   - Processing tool: Flexible analysis on saved data

2. **Performance First**
   - Only compute what user requests via flags
   - Default mode is minimal overhead

3. **Data Preservation**
   - Raw data saved for complete replay capability
   - Multiple analysis passes possible

4. **Consistency**
   - Same interface as Level 1 tool
   - Reuses cli_utils library
   - Familiar patterns for users

5. **Flexibility**
   - Multiple display modes
   - Single or separate file output
   - Configurable depth

## Architecture

### Based On
- Level 1 tool (`retrieve_kraken_live_data_level1`)
- cli_utils library
- Kraken WebSocket client (simdjson)

### New Components
- OrderBookRecord structures
- ChecksumValidator
- OrderBookDisplay (4 modes)
- JsonLinesWriter
- Separate file handling

### Code Reuse
- ✅ CLI argument parsing
- ✅ Input parsing (all 3 methods)
- ✅ WebSocket connection
- ✅ Signal handling
- ✅ Condition variables
- ✅ Symbol mapping

## Future Tools

### Tool 2: `process_orderbook_snapshots`
- Read .jsonl files
- Rebuild order book state
- Create periodic snapshots (1s, 5s, etc.)
- Calculate metrics (spread, imbalance, depth)
- Output CSV for analysis

### Tool 3: `orderbook_replay`
- Visual replay of order book
- Speed control
- Pause/resume
- Jump to timestamp

## Performance Estimates

### Display Overhead
| Mode | CPU | Memory | Use Case |
|------|-----|--------|----------|
| Default | Minimal | Minimal | Many pairs |
| `-v` | Low | Minimal | Moderate pairs |
| `--show-top` | Low | Minimal | Few pairs |
| `--show-book` | Medium | Medium | Single pair |

### File Sizes (BTC/USD, depth 10)
- Per hour: 5-100 MB (depends on volatility)
- 10 pairs: 50-500 MB per hour

## Documentation

- ✅ Complete design document created
- ✅ All decisions documented with rationale
- ✅ Usage examples provided
- ✅ Architecture and implementation notes
- ✅ Testing plan outlined
- ✅ Future tools sketched

**File:** `cpp/docs/LEVEL2_ORDERBOOK_DESIGN.md` (15+ KB)

## Next Steps

1. **Implementation Phase:**
   - Create order book data structures
   - Implement checksum validation
   - Build JSON Lines writer
   - Add WebSocket book channel subscription
   - Implement display modes
   - Add separate files mode
   - Testing

2. **Documentation Updates:**
   - Update PROJECT_STRUCTURE.md when implemented
   - Create usage examples document
   - Add to DOCUMENTATION_INDEX.md (already done)

## Status

- ✅ **Design:** Complete
- ✅ **Documentation:** Complete
- ⏳ **Implementation:** Ready to start
- ⏳ **Testing:** Pending
- ⏳ **Deployment:** Pending

---

**Summary created:** October 3, 2025
**Design document:** `cpp/docs/LEVEL2_ORDERBOOK_DESIGN.md`
**Status:** Ready for implementation
