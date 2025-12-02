#!/bin/bash

# Set the config to start
./set_zswap_config.sh --enabled Y --zpool z3fold --max-pool-percent 40 --compressor lz4hc --shrinker-enabled N

cd build

# Run the tests
./baseline > start.txt