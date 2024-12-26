#!/bin/bash

source conf.cfg

root_path=$ROOT_PATH
errPath="$root_path/data/errors"
log_file="$root_path/data/logs/trrespassRE.log"
output_file="$root_path/data/trrespass-functions.txt"
tmpFile="$root_path/data/tmp/tmpFile.log"
progress="$root_path/data/tmp/stage2.tmp"

banks=$(cat "$root_path/data/banks.txt")
threshold=$(cat "$root_path/data/threshold.txt")
trrespass_threshold="$(expr $threshold \* 3 \/ 5)"

> "$output_file"

echo "TRRespass Started | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" > "$log_file"
echo "----------------------------------------------------" >> "$log_file"

echo "false" > "$tmpFile"

timeout 600 stdbuf -oL taskset 0x1 "$root_path/Tools/TRRespass/obj/tester" -s "$banks" -t "$trrespass_threshold" -v 2>&1 | while IFS= read -r line;
do
    echo "$line" >> "$log_file"
    if [[ "$line" =~ Valid\ Function:\ ([0-9a-fx]+) ]]; then
        echo -e "${BLACK_TXT}${GREEN_BG} TRRespass ${COLOR_RESET} $line"
        hex_value="${BASH_REMATCH[1]}"
        echo "$hex_value    " >> "$output_file"
        echo "true" > "$tmpFile"
    elif [[ "$line" == *"Looking for row bits"* ]]; then
        pkill -SIGINT -P $$
    fi
done

echo "----------------------------------------------------" >> "$log_file"
echo "TRRespass Ended | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$log_file"

echo "82%" > "$progress"
success=$(cat "$tmpFile")
if [[ "$success" == "false" ]];then
  exit 2
fi
