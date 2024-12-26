#!/bin/bash

source conf.cfg

root_path=$ROOT_PATH
errPath="$root_path/data/errors"
log_file="$root_path/data/logs/amdre.log"
output_file="$root_path/data/amdre-functions.txt"
tmpFile="$root_path/data/tmp/tmpFile.log"
unReasonableNumPath="$root_path/data/tmp/tmpFile2.log"
memClass=$(cat "$root_path/data/sys-info/class.txt")
isAmd=$(cat "$root_path/data/isAmd.txt")
progress="$root_path/data/tmp/stage2.tmp"
sTimeAmdre="$root_path/data/tmp/sTimeAmdre.tmp"

runtime_handler() {
    local timeout="$1"
    local sTime=$(cat "$sTimeAmdre")
    while [ -z "$sTime" ]
    do
        sTime=$(cat "$sTimeAmdre")
    done
    local now=$(date +%s)
    local diff=$((now - sTime))
    if [ "$diff" -gt "$timeout" ];then
        return 1
    else
        return 0
    fi
}
echo "$(date +%s)" > "$sTimeAmdre"
echo "AMDRE Started | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" > "$log_file"
echo "----------------------------------------------------" >> "$log_file"

> "$output_file"

success=false
echo "false" > "$tmpFile"

if [ "$isAmd" = "true" ];then
  amdrePath="AMDRE_New"
else
  amdrePath="AMDRE"
fi

if [[ "$memClass" == "DDR3" ]];then
    command="timeout $TIMEOUT_DURATION_AMDRE stdbuf -oL taskset 0x1 $root_path/Tools/$amdrePath/bin/amdre --memory-type=ddr3"
else
    command="timeout $TIMEOUT_DURATION_AMDRE stdbuf -oL taskset 0x1 $root_path/Tools/$amdrePath/bin/amdre"
fi

counter=0
innerCounter=0
loop=true
unReasonableBankNum=true
echo "true" > "$unReasonableNumPath"

while [[ ( "$counter" -lt 3 && "$loop" == "true" ) || "$unReasonableBankNum" == "true" ]]
do
  > "$output_file"
  $command 2>/dev/null | while IFS= read -r line; do
    #if [[ "$line" != *"unrelevant"* && "$line" != *"errors with max"* ]];then
    #  echo "$line"
    #fi
    echo "$line"
    echo "$line" >> "$log_file"
    if [[ "$line" =~ Address\ Function:\ 0x([0-9a-fA-F]+)\ seems\ to\ be\ valid ]]; then
      # Extract the hexadecimal value using BASH_REMATCH
      hex_value="${BASH_REMATCH[1]}"
      echo "0x$hex_value    " >> "$output_file"
      echo "true" > "$tmpFile"
    fi
    if [[ "$line" == *"does not match the number of banks"* ]];then
      echo "false" > "$tmpFile"
    fi
    if [[ "$line" == *"Assuming"* ]] && [[ "$line" == *"banks"* ]];then
      b=$(echo "$line" | grep -oP '(?<=Assuming )\d+(?= banks)')
      if [[ "$b" -lt 8 ]]; then
        echo "true" > "$unReasonableNumPath"
        killall amdre > /dev/null 2>&1
        sleep 1
        break
      else
        echo "false" > "$unReasonableNumPath"
      fi
    fi
  done
  unReasonableBankNum=$(cat "$unReasonableNumPath")
  success=$(cat "$tmpFile")
  if [[ "$success" == "false" ]];then
    loop="true"
    if [ "$unReasonableBankNum" = "false" ];then
      counter=$((counter + 1))
    fi
    if [ "$counter" -lt 3 ];then
      echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Failed to retrieve the addressing functions. Trying Again ... ${RED_BG} TRY $counter ${COLOR_RESET}"
    else
      echo -e "${RED_BG} REAF ${COLOR_RESET} AMDRE Failed ${COLOR_RESET}"
    fi
  else
    loop="false"
  fi
  runtime_handler "3600"
  runtime_handler_status=$?
  if [ "$runtime_handler_status" = "1" ]; then
    break
  fi
done

echo "----------------------------------------------------" >> "$log_file"
echo "AMDRE Ended | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$log_file"

echo "75%" > "$progress"

success=$(cat "$tmpFile")
if [[ "$success" == "false" ]];then
  exit 2
fi
