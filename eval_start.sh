#!/bin/bash

cd build

# Set the config to start
sudo ./set_zswap_config.sh --enabled Y --zpool z3fold --max-pool-percent 40 --compressor lz4hc --shrinker-enabled N

# Run the tests
./baseline > start.txt