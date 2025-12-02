#!/bin/bash

cd build

# Set the config to start
sudo ./set_zswap_config.sh --enabled Y --zpool zsmalloc --max-pool-percent 10 --compressor 842 --shrinker-enabled Y

# Run the tests
./baseline > result.txt