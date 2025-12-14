#!/bin/bash

# Set the config to start
./set_zswap_config.sh --enabled Y --zpool zbud --max-pool-percent 10 --compressor lzo --shrinker-enabled N

cd build

# Run the tests
./baseline > start.txt
