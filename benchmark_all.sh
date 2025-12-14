#!/bin/bash

# Benchmark script to run evolution search for different max_pool_percent values
# This script tests all possible max_pool_percent values (10, 20, 30, 40, 50, 60)
# with the evolution algorithm to find optimal ZSWAP configurations.
# It waits for memory utilization to fall below 35% before starting each new run.

set -e  # Exit on any error

# Function to get current memory utilization percentage
get_memory_utilization() {
    # Read total and available memory from /proc/meminfo
    local total_kb=$(grep "^MemTotal:" /proc/meminfo | awk '{print $2}')
    local available_kb=$(grep "^MemAvailable:" /proc/meminfo | awk '{print $2}')

    # Calculate used memory
    local used_kb=$((total_kb - available_kb))

    # Calculate percentage (integer division)
    local utilization=$(( (used_kb * 100) / total_kb ))

    echo "$utilization"
}

# Function to wait for memory utilization to drop below threshold
wait_for_memory_clearance() {
    local threshold=$1
    local check_interval=5   # Check every 5 seconds

    echo "Waiting for memory utilization to drop below ${threshold}%..."

    while true; do
        local current_util=$(get_memory_utilization)
        echo "Current memory utilization: ${current_util}% (threshold: ${threshold}%)"

        if [ $current_util -lt $threshold ]; then
            echo "Memory utilization is now below ${threshold}%. Proceeding..."
            return 0
        fi

        echo "Waiting ${check_interval} seconds before checking again..."
        sleep $check_interval
    done
}

cd build

# Configuration
SUBSET_SIZE=20
BASE_OUTPUT_PATH="/home/ubuntu/hnswlib/results"
EXECUTABLE="./main"

# Array of max_pool_percent values to test
MAX_POOL_VALUES=(10 20 30 40 50 60)
MEMORY_THRESHOLD=35  # Wait until memory utilization drops below 35%

echo "Starting benchmark for all max_pool_percent values..."
echo "Subset size: $SUBSET_SIZE"
echo "Base output path: $BASE_OUTPUT_PATH"
echo "Memory threshold for next run: ${MEMORY_THRESHOLD}%"
echo "========================================"

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Executable '$EXECUTABLE' not found. Please build the project first."
    exit 1
fi

# Create base output directory if it doesn't exist
mkdir -p "$BASE_OUTPUT_PATH"

# Loop through each max_pool_percent value
for i in "${!MAX_POOL_VALUES[@]}"; do
    max_pool=${MAX_POOL_VALUES[$i]}

    # Wait for memory clearance before starting (except for the first run)
    if [ $i -gt 0 ]; then
        echo ""
        echo "========================================"
        echo "Memory clearance check before next run"
        echo "========================================"
        wait_for_memory_clearance $MEMORY_THRESHOLD
    fi

    echo ""
    echo "========================================"
    echo "Testing max_pool_percent = $max_pool"
    echo "========================================"

    # Create output directory for this specific max_pool_percent value
    OUTPUT_DIR="$BASE_OUTPUT_PATH/max_pool_$max_pool"
    mkdir -p "$OUTPUT_DIR"

    echo "Output directory: $OUTPUT_DIR"
    echo "Command: $EXECUTABLE --max-pool-percent=$max_pool --subset-size=$SUBSET_SIZE --output-path=$OUTPUT_DIR"

    # Run the evolution search
    # Using 'time' to measure execution time
    time "$EXECUTABLE" "--max-pool-percent=$max_pool" "--subset-size=$SUBSET_SIZE" "--output-path=$OUTPUT_DIR"

    echo "Completed evolution for max_pool_percent = $max_pool"
    echo "Results saved in: $OUTPUT_DIR"
done

echo ""
echo "========================================"
echo "Benchmark completed!"
echo "All results saved in: $BASE_OUTPUT_PATH"
echo "========================================"

# Optional: Create a summary of all results
echo ""
echo "Summary of results:"
for max_pool in "${MAX_POOL_VALUES[@]}"; do
    OUTPUT_DIR="$BASE_OUTPUT_PATH/max_pool_$max_pool"

    # Check if any generation files were created
    if ls "$OUTPUT_DIR"/best_candidate_gen_*.json 1> /dev/null 2>&1; then
        # Get the latest generation file
        LATEST_FILE=$(ls "$OUTPUT_DIR"/best_candidate_gen_*.json | sort -V | tail -n 1)
        echo "max_pool_$max_pool: $LATEST_FILE"
    else
        echo "max_pool_$max_pool: No results found"
    fi
done
