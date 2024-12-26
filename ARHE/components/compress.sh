#!/bin/bash

source conf.cfg
root_path=$ROOT_PATH
log_file="$root_path/data/logs/compress.log"
fileNamePath="$root_path/data/tmp/zipFileName.tmp"
compressProgress="$root_path/data/tmp/compressProgress.tmp"
echo "0" > "$compressProgress"

token=$(cat "$root_path/data/token.txt")
cpuModel=$(cat /proc/cpuinfo | grep "model name" | head -n1 | awk -F: '{print $2}' | sed -e 's/^[ \t]*//' -e 's/[ \t]*$//' -e 's/ /_/g' -e 's/(R)//g' -e 's/(TM)//g' -e 's/(tm)//g' -e 's/(r)//g' -e 's/@.*//' -e 's/_CPU//g' -e 's/_AMD//g' -e 's/_Intel//g' -e 's/_*$//')
cpuModel=$(echo "$cpuModel" | sed 's/\///g')  # Removes all '/'
fileName="$(date +%Y-%m-%d-%H-%M-%S)_${token}_${cpuModel}.zip"
echo "$fileName" > "$fileNamePath"

cd "$root_path"
echo -e "${BLUE_BG} Compressor ${COLOR_RESET} Compressing the results ..."
zip -r "$fileName" data -x "data/tmp/*" "*.zip" > "$log_file" 2>&1
echo -e "${BLUE_BG} Compressor ${COLOR_RESET} Compressing is done!"
echo "100" > "$compressProgress"
