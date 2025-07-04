#!/bin/bash

# Change to OpenCAS root directory
cd /home/chanseo/open-cas-linux

# Build the module
echo "Building kernel module..."
make -C modules/cas_cache CFLAGS="-Werror -Wall" M=$(pwd)/modules/cas_cache 2>&1 | grep -A 5 -B 5 test_lookup

echo "Building userspace test..."
cd ocf/src/utils/pmem_nvme
make clean && make

echo "Running userspace test..."
./test_lookup

echo "Done." 