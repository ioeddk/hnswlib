#!/bin/bash

# Set the config to start
./set_zswap_config.sh --enabled Y --zpool zbud --max-pool-percent 10 --compressor lz4hc --shrinker-enabled Y

cd build

# Run the tests
./baseline > result.txt
