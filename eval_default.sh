#!/bin/bash


# Set the config to start
./set_zswap_config.sh --enabled Y --zpool zbud --max-pool-percent 20 --compressor lzo --shrinker-enabled Y

# Run the tests
cd build

./baseline > default.txt