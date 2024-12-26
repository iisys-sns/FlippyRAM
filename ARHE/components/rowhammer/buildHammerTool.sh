#!/bin/bash

source conf.cfg

# Set file paths required for the execution
root_path=$ROOT_PATH
buildLogFile="$root_path/data/logs/build.log"

# Load required information from files, e.g. addressing functions, number of banks, etc.
banks=$(cat "$root_path/data/banks.txt")
functions=$(cat "$root_path/data/final-functions.txt")

# Write that building of the tool starts to the log file
echo "Start building HammerTool | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogFile"

# Adjust required information in the source file (for HammerTool, this is an
# additional config file, might be some header or c file for other tools)
echo "banks=${banks}" > $root_path/Tools/HammerTool/config.cfg
echo "masks=${functions}" >> $root_path/Tools/HammerTool/config.cfg

# Change into the directory of the tool and build it using make, cmake, etc.
cd "$root_path/Tools/HammerTool/"
make >> "$buildLogFile" 2>&1

# Check the return code of the build process. If it failed, write that to the
# log and exit with an error code
if [ $? -ne 0 ]; then
	echo "Failed building HammerTool | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogFile"
	exit 1
fi

# Build was successful, exit with code 0
echo "Done building HammerTool | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogFile"
exit 0
