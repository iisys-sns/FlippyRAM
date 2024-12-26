#!/bin/bash

source conf.cfg

root_path=$ROOT_PATH
bitFlipLog="$root_path/data/bit-flip-reports-rowpress.txt"
bitFlipCounter="$root_path/data/tmp/rowpressBitFlipsNum.tmp"
TIMEOUT_DURATION_HAMMERTOOL=$(cat "$root_path/data/tmp/available_runtime.tmp")
progressStage4="$root_path/data/tmp/stage4.tmp"
echo "false" > "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp"
echo "false" > "$root_path/data/tmp/rowhammerToolEnded.tmp"

SCRIPT_START=$(date +%s)
end_timestamp=$(( SCRIPT_START + TIMEOUT_DURATION_HAMMERTOOL ))

# This is a function to calculate percentage based on timeout duration
progress_calculator(){
    local now
    local timePassed
    local int_progress
    local progress=0
    local min_percentage=$(cat "$progressStage4" | tr -cd '0-9\n')
    local max_percentage=$(cat "$root_path/data/tmp/set_max_percentage.tmp")
    local status=$(cat "$root_path/data/tmp/rowhammerToolEnded.tmp")
    while [ "$status" = "true" ]
    do
        sleep 1
        status=$(cat "$root_path/data/tmp/rowhammerToolEnded.tmp")
    done
    while [ "$progress" -lt "$max_percentage" ]; do
        now=$(date +%s)
        timePassed=$((now - SCRIPT_START))

        # Calculate the current percentage based on time passed
        progress=$(( (timePassed * max_percentage) / TIMEOUT_DURATION_HAMMERTOOL ))
        progress=$(( progress + min_percentage ))

        progress=$(( progress / 1 ))
        # Ensure percentage stays within bounds
        if [ "$progress" -gt "$max_percentage" ]; then
            progress=$max_percentage
        fi

        # Write the progress percentage to the file
        echo "$progress%" > "$progressStage4"

        status=$(cat "$root_path/data/tmp/rowhammerToolEnded.tmp")

        if [ "$status" = "true" ];then
            break
        fi

        sleep 1
    done
    echo "$max_percentage%" > "$progressStage4"
}

# Run the progress calculator in the background
(
    progress_calculator
) &

echo "0" > "$bitFlipCounter"

cd $root_path/Tools/RowPress

runtime=$(($end_timestamp - $(date +%s)))
while [ "$runtime" -gt 60 ]; do
	runOutputFile="$root_path/data/logs/rowpress-$(date +%Y-%m-%d_%H-%M-%S).log"
	echo "RowPress Started | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$runOutputFile"
	echo "----------------------------------------------------" >> "$runOutputFile"
	echo "Starting RowPress. Logging output to $runOutputFile" >> "$bitFlipLog"

	echo "timeout --kill-after=5 $runtime stdbuf -oL ./demo-algo2 --num_victims 1500 2>&1" >> "$runOutputFile"
	timeout --kill-after=5 $runtime stdbuf -oL ./demo-algo2 --num_victims 1500 2>&1 | while IFS= read -r line;
	do
		echo "$line"
		echo "$line" >> "$runOutputFile"

		if [[ "$line" == *"with bit flip count"* ]];then
            bit_flip_count=$(echo "$line" | grep -oP '(?<=with bit flip count )\d+')
            if [ "$bit_flip_count" -gt 0 ]; then
                echo "Bit Flip Found | $(date +%Y-%m-%d_%H-%M-%S)" >> "$bitFlipLog"
                echo "$line" >> "$bitFlipLog"
                echo "-------------------------------------------" >> "$bitFlipLog"
                bitFlips=$(cat "$bitFlipCounter")
                bitFlips=$((bitFlips + bit_flip_count))
                echo "$bitFlips" > "$bitFlipCounter"
            fi
		fi

        # Handle Dumped Core Error
		echo "false" > "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp"
		if [[ "$line" == *"monitored command dumped core"* ]];then
			echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} RowPress Dumped Core. Stopping experiment..."
			echo "true" > "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp"
			killall -9 "demo-algo2" > /dev/null 2>&1
		fi

		fileSize="$(stat "$runOutputFile"  | grep "Size" | cut -d ":" -f 2 | sed 's/^ *\([0-9]\+\) \+.*$/\1/g')"
		if [ "$fileSize" -ge "$FILE_SIZE_LIMIT" ]; then
			echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} RowPress exceeded maximum log file size. Stopping experiment..."
			killall -9 "demo-algo2" > /dev/null 2>&1
			exit 1
		fi

	done
	echo "----------------------------------------------------" >> "$runOutputFile"
	echo "RowPress Ended | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$runOutputFile"
    dumped_core=$(cat "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp")
    if [ "$dumped_core" = "true" ];then
        echo  "[$(date +%Y-%m-%d_%H-%M-%S)] | RowPress Dumped Core!" >> "$runOutputFile"
		break
	fi
	runtime=$(($end_timestamp - $(date +%s)))
done

cd "$root_path"

bitFlips=$(cat "$bitFlipCounter")
echo "Total Bit Flips: $bitFlips" >> "$bitFlipLog"

echo "true" > "$root_path/data/tmp/rowhammerToolEnded.tmp"

# Exit the script with correct code
if [ "$dumped_core" = "true" ];then
	exit 1
else
	exit 0
fi
