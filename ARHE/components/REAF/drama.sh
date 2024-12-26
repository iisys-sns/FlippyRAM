#!/bin/bash

source conf.cfg

root_path=$ROOT_PATH
log_file="$root_path/data/logs/drama.log"
output_file="$root_path/data/tmp/temp-addr-drama.log"
errPath="$root_path/data/errors"
tmpFile="$root_path/data/tmp/tmpFile.log"
progress="$root_path/data/tmp/stage2.tmp"

echo "DRAMA Started | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" > "$log_file"
echo "----------------------------------------------------" >> "$log_file"

> "$output_file"

banks=$(cat "$root_path/data/banks.txt")
channelNum=$(cat "$root_path/data/sys-info/dimm-numbers.txt")
sets=$((banks * channelNum))
percentage_counter=1
capture=false
functions=0
success=false
echo "$success" > "$tmpFile"
retry_counter=0

while [[ "$success" == false && "$retry_counter" < 3 ]];
do
  timeout "${TIMEOUT_DURATION_DRAMA}s" stdbuf -oL taskset 0x1 "$root_path/Tools/Drama/measure" -s "$sets" 2>&1 | while IFS= read -r line; 
  do
    if [[ "$line" == *"Searching"* && "$line" == *"try 1"* ]]; then
      percentage=$(awk "BEGIN { printf \"%.2f\", ($percentage_counter / $sets) * 100 }")
      percentage_counter=$((percentage_counter + 1))
      echo -e "${BLACK_TXT}${GREEN_BG} DRAMA ${COLOR_RESET} Measuring: ${percentage}%"
    fi
    echo "$line" >> "$log_file"
    if [[ "$line" == *"reduced"* && "$line" == *"functions"* ]]; then
      if [ "$percentage" != 100 ];then
        echo -e "${BLACK_TXT}${GREEN_BG} DRAMA ${COLOR_RESET} Measuring: DONE!"
      fi
      echo -e "${BLACK_TXT}${GREEN_BG} DRAMA ${COLOR_RESET} $line | ${GREEN_TXT}This may take a while...${COLOR_RESET} "
      capture=true
    fi
    if [[ "$capture" == true ]]; then
      if [[ "$line" == *"Correct"* ]]; then
        echo "true" > "$tmpFile"
        echo -e "${BLACK_TXT}${GREEN_BG} DRAMA ${COLOR_RESET} ${GREEN_TXT}Function found${COLOR_RESET}: ${line}"
        echo  "$line      " >> "$output_file"
      fi
    fi
  done
  status=$?
  if [ "$status" -eq 124 ]; then
    echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET}${RED_BG} timeout reached! ${COLOR_RESET}"
    echo  "[$(date +%Y-%m-%d_%H-%M-%S)] | timeout reached!" >> "$errPath/drama.log"
  fi
  success=$(cat "$tmpFile")
  if [[ "$success" == true ]];then
    echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Addressing functions retrieved successfully!"
    break
  else
    retry_counter=$((retry_counter + 1))
    rr=$((retry_counter + 1))
    percentage_counter=1
    echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Retrieving functions failed. Trying again ... ${RED_BG} try ${rr} ${COLOR_RESET}"
  fi
done

echo "----------------------------------------------------" >> "$log_file"
echo "DRAMA Ended | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$log_file"

echo "55%" > "$progress"

if [[ "$success" == false ]];then
  exit 1
fi
