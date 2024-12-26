#!/bin/bash

source conf.cfg

root_path=$ROOT_PATH
log_file="$root_path/data/logs/dare.log"
output_file="$root_path/data/dare-functions.txt"
errPath="$root_path/data/errors"
tmpFile="$root_path/data/tmp/tmpFile.log"
progress="$root_path/data/tmp/stage2.tmp"

echo "Dare (Zenhammer) Started | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" > "$log_file"
echo "----------------------------------------------------" >> "$log_file"

> "$output_file"

bash "$root_path/components/utils/hugepage.sh"

percentage_counter=1
steps=10
capture=false
functions=0
success=false
echo "$success" > "$tmpFile"
retry_counter=0
threshold=$(cat "$root_path/data/threshold.txt")
dare_threshold="$(expr $threshold \* 4 \/ 5)"
isAmd=$(cat "$root_path/data/isAmd.txt")
banks=$(cat "$root_path/data/banks.txt")

while [[ "$success" == false && "$retry_counter" < 3 ]];
do
	if [ "$isAmd" = "true" ];then
    offset=$(timeout -s 9 5h sudo cat /proc/iomem | python3 scripts/get_amd_zen_offset.py  | grep "Make DARE consider" | cut -d " " -f 12)
  else
    offset=0
  fi

	inBankFunctions=0
  timeout --signal=KILL "${TIMEOUT_DURATION_DARE}s" stdbuf -oL "$root_path/Tools/dare/build/dare" --superpages 1 --clusters $banks --offset $offset 2>&1 | while IFS= read -r line; 
  do
    echo "$line" >> "$log_file"
		# TODO: Monitor line timestamps in Dare output to calculate intermediate progress
		# (not implemented yet)

		# TODO: Not sure if there is some bad case where Dare does not work.
    #if [[ "$line" == *"does not match the number of banks"* ]];then
    #  echo "false" > "$tmpFile"
    #fi

		if [[ "$line" =~ "XOR of all found functions:" ]]; then
			inBankFunctions=0
			continue
		fi

		if [[ "$line" =~ "Found "[0-9]+" functions" ]]; then
			inBankFunctions=1
			continue
		fi

		if [ "$inBankFunctions" -eq 1 ]; then
			if [ "$bankFunctions" != "" ]; then
				bankFunctions="$bankFunctions,"
			fi
			line="$(echo "$line" | sed 's/0x0*\([0-9a-f]\+\) .*$/0x\1/g')"
			bankFunctions="$bankFunctions$line"
			echo "true" > "$tmpFile"
			echo  "$bankFunctions" > "$output_file"
		fi
  done

  status=$?
  if [ "$status" -eq 124 ]; then
    echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET}${RED_BG} timeout reached! ${COLOR_RESET}"
    echo  "[$(date +%Y-%m-%d_%H-%M-%S)] | timeout reached!" >> "$errPath/dare.log"
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
echo "Dare (Zenhammer) Ended | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$log_file"

echo "96%" > "$progress"

if [[ "$success" == false ]];then
  exit 1
fi
