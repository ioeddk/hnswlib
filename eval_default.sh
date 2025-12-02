#!/bin/bash

cd build

# Set the config to start
sudo ./set_zswap_config.sh --enabled Y --zpool zbud --max-pool-percent 20 --compressor lzo --shrinker-enabled Y

# Run the tests
./baseline > default.txt