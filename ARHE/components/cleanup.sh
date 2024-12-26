#!/bin/bash

source conf.cfg
root_path=$ROOT_PATH
zipFile="$root_path/data/tmp/zipFileName.tmp"
zipFileName=$(cat "$zipFile")
zipFile="$root_path/$zipFileName"

if [ -f "/.dockerenv" ];then
    mv "$zipFile" "$root_path/data"
    if [ -f "$root_path/tokens.txt" ]; then
        mv "$root_path/tokens.txt" /arhe/ARHE/data/
    fi
    find "$root_path" -mindepth 1 -type f ! -name "*.zip" ! -name "tokens.txt" -delete > /dev/null 2>&1
    find "$root_path" -mindepth 1 -type d ! -path "$root_path/data" -exec rm -rf {} + > /dev/null 2>&1
    find "$root_path/data" -mindepth 1 -type f ! -name "*.zip" ! -name "tokens.txt" -delete > /dev/null 2>&1
    find "$root_path/data" -mindepth 1 -type d -exec rm -rf {} + > /dev/null 2>&1

else
    find "$root_path" -mindepth 1 -type f ! -name "*.zip" ! -name "tokens.txt" -delete > /dev/null 2>&1
    find "$root_path" -mindepth 1 -type d -exec rm -rf {} + > /dev/null 2>&1
fi
