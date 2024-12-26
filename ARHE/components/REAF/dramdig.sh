#!/bin/bash

source conf.cfg

root_path=$ROOT_PATH
log_file="$root_path/data/logs/dramdig.log"
output_file="$root_path/data/tmp/dramdig-functions.txt"
errPath="$root_path/data/errors"
tmpFile="$root_path/data/tmp/tmpFile.log"
progress="$root_path/data/tmp/stage2.tmp"

echo "DRAMDig Started | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" > "$log_file"
echo "----------------------------------------------------" >> "$log_file"

> "$output_file"

percentage_counter=1
steps=10
capture=false
functions=0
success=false
echo "$success" > "$tmpFile"
retry_counter=0

while [[ "$success" == false && "$retry_counter" < 3 ]];
do
	inBankFunctions=0
	bankFunctions=""

	currentDir="$(pwd)"
	cd "$root_path/Tools/DRAMDig/"
  timeout "${TIMEOUT_DURATION_DRAMDIG}s" stdbuf -oL taskset 0x1 ./dramdig 2>&1 | while IFS= read -r line; 
  do
    echo "$line" >> "$log_file"
		# TODO: Monitor line timestamps in DRAMDig output to calculate intermediate progress
		# (not implemented yet)

		# TODO: Not sure if there is some bad case where DRAMDig does not work.
    #if [[ "$line" == *"does not match the number of banks"* ]];then
    #  echo "false" > "$tmpFile"
    #fi

		if [[ "$line" =~ "=== bank addr funs ("[0-9]+"): ===" ]]; then
			inBankFunctions=1
			continue
		fi

		if [[ "$line" =~ "===" ]] || [[ "$line" =~ "[+]" ]]; then
			inBankFunctions=0
			continue
		fi


		if [ "$inBankFunctions" -eq 1 ]; then
			if [ "$bankFunctions" != "" ]; then
				bankFunctions="$bankFunctions,"
			fi
			line="$(echo "$line" | sed 's/,$//g')"
			bankFunctions="$bankFunctions$(python -c "$(echo -e "num=0\nfor p in [$line]: num += 2**p\nprint(str(hex(num)))")")"
			echo "true" > "$tmpFile"
			echo  "$bankFunctions" > "$output_file"
		fi
  done

  status=$?
  if [ "$status" -eq 124 ]; then
    echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET}${RED_BG} timeout reached! ${COLOR_RESET}"
    echo  "[$(date +%Y-%m-%d_%H-%M-%S)] | timeout reached!" >> "$errPath/dramdig.log"
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
echo "DRAMDig Ended | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$log_file"

cd $currentDir

echo "91%" > "$progress"

if [[ "$success" == false ]];then
  exit 1
fi
