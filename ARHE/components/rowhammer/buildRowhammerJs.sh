#!/bin/bash

source conf.cfg

# Set file paths required for the execution
root_path=$ROOT_PATH
buildLogFile="$root_path/data/logs/build.log"

# Load required information from files, e.g. addressing functions, number of banks, etc.
banks=$(cat "$root_path/data/banks.txt")
functions=$(cat "$root_path/data/final-functions.txt")

# Write that building of the tool starts to the log file
echo "Start building Rowhammer.JS | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogFile"

# Change into the directory of the tool and build it using make, cmake, etc.
cd "$root_path/Tools/rowhammerjs/"

sed -i 's/^\(#define N_BANKS (\).*$/\1 '"$banks"' )/g' rowhammer.cc
sed -i 's/^\(uint64_t afns\[\] = {\) .*$/\1 '"$functions"'};/g' rowhammer.cc

make >> "$buildLogFile" 2>&1

# Check the return code of the build process. If it failed, write that to the
# log and exit with an error code
if [ $? -ne 0 ]; then
	echo "Failed building Rowhammer.js | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogFile"
	exit 1
fi

# Build was successful, exit with code 0
echo "Done building Rowhammer.js | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogFile"
exit 0
