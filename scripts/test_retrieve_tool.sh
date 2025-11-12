#!/bin/bash
# Test script for retrieve_kraken_live_data_level1
work_dir=$PWD

TOOL="./cpp/build/retrieve_kraken_live_data_level1"

echo "=========================================="
echo "Testing Kraken Live Data Retriever - Level 1"
echo "=========================================="
echo ""

# Test 1: Help
echo "Test 1: Help message"
echo "Command: $TOOL -h"
echo "------------------------------------------"
cd $work_dir
$TOOL -h
echo ""
echo ""

# Test 2: Direct pair list
echo "Test 2: Direct pair list (comma-separated)"
echo "Command: $TOOL -p \"BTC/USD,ETH/USD,SOL/USD\""
echo "Running for 10 seconds..."
echo "------------------------------------------"
cd $work_dir
timeout 10 $TOOL -p "BTC/USD,ETH/USD,SOL/USD" 2>&1 | head -30
echo ""
echo "[Test terminated after 10 seconds]"
echo ""
echo ""

# Test 3: CSV file with limit
echo "Test 3: CSV file input with limit"
echo "Command: $TOOL -p ./data/kraken_usd_volume.csv:pair:5"
echo "Running for 10 seconds..."
echo "------------------------------------------"

cd $work_dir
timeout 10 $TOOL -p ./data/kraken_usd_volume.csv:pair:5 2>&1 | head -30
echo ""
echo "[Test terminated after 10 seconds]"
echo ""
echo ""

# Test 4: Invalid input
echo "Test 4: Error handling (missing -p argument)"
echo "Command: $TOOL"
echo "------------------------------------------"
cd $work_dir
$TOOL 2>&1 | head -10
echo ""
echo ""

# Test 5: Invalid CSV column
echo "Test 5: Error handling (invalid CSV column)"
echo "Command: $TOOL -p ./data/kraken_usd_volume.csv:invalid_column:5"
echo "------------------------------------------"
cd $work_dir
$TOOL -p ./data/kraken_usd_volume.csv:invalid_column:5 2>&1 | head -10
echo ""
echo ""

echo "=========================================="
echo "All tests completed!"
echo "=========================================="
