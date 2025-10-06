# Documentation Index

## Overview

All documentation for the Kraken WebSocket C++ project is organized in the `cpp/docs/` directory, following a clear structure based on topics.

## Quick Navigation

### 🚀 Start Here
1. **[EXAMPLES_README.md](EXAMPLES_README.md)** - How to use the example applications
2. **[PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md)** - Project organization and architecture
3. **[QUICK_REFERENCE.md](QUICK_REFERENCE.md)** - API quick reference

### 📚 Core Documentation

#### Tool Design Documents
- **[LEVEL2_ORDERBOOK_DESIGN.md](LEVEL2_ORDERBOOK_DESIGN.md)** - Level 2 order book tool design and decisions

#### WebSocket Client Library
- **[README.md](README.md)** - Original setup guide
- **[README_REFACTORED.md](README_REFACTORED.md)** - Refactored code guide
- **[BLOCKING_VS_NONBLOCKING.md](BLOCKING_VS_NONBLOCKING.md)** - Comparison of blocking vs non-blocking approaches
- **[DEVELOPMENT_NOTES.md](DEVELOPMENT_NOTES.md)** - Build issues and troubleshooting

#### Refactoring & Architecture
- **[REFACTORING.md](REFACTORING.md)** - Refactoring process and decisions
- **[RESTRUCTURING_SUMMARY.md](RESTRUCTURING_SUMMARY.md)** - Project restructuring summary
- **[TEMPLATE_REFACTORING.md](TEMPLATE_REFACTORING.md)** - Template-based refactoring

#### JSON Libraries
- **[JSON_LIBRARY_COMPARISON.md](JSON_LIBRARY_COMPARISON.md)** - Performance comparison of JSON libraries
- **[SIMDJSON_MIGRATION.md](SIMDJSON_MIGRATION.md)** - Migration plan to simdjson
- **[SIMDJSON_USAGE_GUIDE.md](SIMDJSON_USAGE_GUIDE.md)** - How to use simdjson
- **[SIMDJSON_IMPLEMENTATION_SUMMARY.md](SIMDJSON_IMPLEMENTATION_SUMMARY.md)** - Implementation details

#### Technical Analysis
- **[FLOAT_VS_DOUBLE_ANALYSIS.md](FLOAT_VS_DOUBLE_ANALYSIS.md)** - Precision analysis
- **[PERFORMANCE_EXPLANATION.md](PERFORMANCE_EXPLANATION.md)** - Performance optimization details

#### Project Overview
- **[SUMMARY.md](SUMMARY.md)** - Project overview and summary

### 🛠️ CLI Utils Library Documentation

#### Quick Start
- **[CLI_UTILS_QUICK_REFERENCE.md](CLI_UTILS_QUICK_REFERENCE.md)** - ⭐ API quick reference

#### Features & Usage
- **[CLI_UTILS_FEATURE_SUMMARY.md](CLI_UTILS_FEATURE_SUMMARY.md)** - Overview and benefits
- **[CLI_UTILS_TEXT_FILE_SUPPORT.md](CLI_UTILS_TEXT_FILE_SUPPORT.md)** - Text file support feature

#### Development
- **[CLI_UTILS_REFACTORING.md](CLI_UTILS_REFACTORING.md)** - Refactoring summary and history

## Documentation by Purpose

### For New Users
Start with these in order:
1. [EXAMPLES_README.md](EXAMPLES_README.md)
2. [QUICK_REFERENCE.md](QUICK_REFERENCE.md)
3. [CLI_UTILS_QUICK_REFERENCE.md](CLI_UTILS_QUICK_REFERENCE.md)
4. [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md)

### For Building Tools
- [CLI_UTILS_FEATURE_SUMMARY.md](CLI_UTILS_FEATURE_SUMMARY.md) - Learn about CLI utilities
- [CLI_UTILS_QUICK_REFERENCE.md](CLI_UTILS_QUICK_REFERENCE.md) - API reference
- [EXAMPLES_README.md](EXAMPLES_README.md) - Example patterns

### For Understanding Performance
- [JSON_LIBRARY_COMPARISON.md](JSON_LIBRARY_COMPARISON.md)
- [SIMDJSON_USAGE_GUIDE.md](SIMDJSON_USAGE_GUIDE.md)
- [PERFORMANCE_EXPLANATION.md](PERFORMANCE_EXPLANATION.md)
- [FLOAT_VS_DOUBLE_ANALYSIS.md](FLOAT_VS_DOUBLE_ANALYSIS.md)

### For Architecture & Design
- [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md)
- [BLOCKING_VS_NONBLOCKING.md](BLOCKING_VS_NONBLOCKING.md)
- [REFACTORING.md](REFACTORING.md)
- [TEMPLATE_REFACTORING.md](TEMPLATE_REFACTORING.md)

### For Troubleshooting
- [DEVELOPMENT_NOTES.md](DEVELOPMENT_NOTES.md) - Build issues
- [README.md](README.md) - Setup guide
- [QUICK_REFERENCE.md](QUICK_REFERENCE.md) - API reference

## Tool-Specific Documentation

Located in `cpp/examples/`:
- **README_retrieve_kraken_live_data_level1.md** - Tool documentation
- **README_retrieve_kraken_live_data_level1_USAGE.md** - Detailed usage examples

## Documentation Standards

### File Naming Convention
- **ALL_CAPS.md** - Main documentation files
- **CLI_UTILS_*.md** - CLI utilities library documentation
- **README*.md** - Getting started guides

### Priority Levels
- ⭐⭐⭐ - **Essential**: Read first
- ⭐⭐ - **Important**: Read for full understanding
- ⭐ - **Reference**: Read as needed

## Recent Additions

### CLI Utils Library (October 2025)
- CLI_UTILS_FEATURE_SUMMARY.md
- CLI_UTILS_QUICK_REFERENCE.md
- CLI_UTILS_REFACTORING.md
- CLI_UTILS_TEXT_FILE_SUPPORT.md

### Production Tools
- retrieve_kraken_live_data_level1 documentation

## Contributing to Documentation

When adding new documentation:

1. **Location**:
   - General docs → `cpp/docs/`
   - Tool-specific docs → `cpp/examples/`
   - Library-specific docs → `cpp/docs/` with prefix (e.g., `CLI_UTILS_*`)

2. **Naming**:
   - Use ALL_CAPS for main files
   - Use descriptive prefixes for library docs
   - Include version/type suffixes when needed

3. **Update**:
   - Add entry to this index
   - Update PROJECT_STRUCTURE.md if needed
   - Cross-reference related docs

## File Count Summary

Total documentation files: **23**

**Core Documentation**: 14 files
- Tool design: 1 file
- WebSocket client: 5 files
- Refactoring: 3 files
- JSON libraries: 4 files
- Technical: 2 files
- Overview: 1 file

**CLI Utils Documentation**: 4 files
**Tool Documentation**: 2 files (in examples/)
**Index**: 1 file (this file)
**Structure**: 1 file (PROJECT_STRUCTURE.md)

## External Documentation

- **Kraken API Docs**: https://docs.kraken.com/api/
- **WebSocket++ Docs**: https://www.zaphoyd.com/websocketpp/
- **simdjson Docs**: https://github.com/simdjson/simdjson
- **nlohmann/json Docs**: https://json.nlohmann.me/

## Maintenance

This index is maintained alongside the documentation. Last updated: October 2025.

For documentation questions or improvements, refer to the issue tracker or update this index when adding new files.
