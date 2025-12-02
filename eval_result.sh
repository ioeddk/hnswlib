#!/bin/bash

# Set the config to start
./set_zswap_config.sh --enabled Y --zpool zsmalloc --max-pool-percent 10 --compressor 842 --shrinker-enabled Y

cd build

# Run the tests
./baseline > result.txt