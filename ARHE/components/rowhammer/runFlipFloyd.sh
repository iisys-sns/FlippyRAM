#!/bin/bash

source conf.cfg

# Set file paths required for the execution
root_path=$ROOT_PATH
bitFlipLog="$root_path/data/bit-flip-reports-flipfloyd.txt"
bitFlipCounter="$root_path/data/tmp/flipfloydBitFlipsNum.tmp"
TIMEOUT_DURATION_HAMMERTOOL=$(cat "$root_path/data/tmp/available_runtime.tmp")
progressStage4="$root_path/data/tmp/stage4.tmp"
SCRIPT_START=$(date +%s)
end_timestamp=$(( SCRIPT_START + TIMEOUT_DURATION_HAMMERTOOL ))
echo "false" > "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp"
echo "false" > "$root_path/data/tmp/rowhammerToolEnded.tmp"

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

# Initialize the bit flip counter of the experiment to zero
echo "0" > "$bitFlipCounter"

# Change into the directory of the tool (might be required for some tools)
cd $root_path/Tools/flipfloyd

# Loop with timeout until the runtime is exhausted
runtime=$(($end_timestamp - $(date +%s)))
while [ "$runtime" -gt 60 ]; do
	# Get the amount of available memory and use half of it as memory size for FlipFloyd
	availableMemory="$(free -h | sed 's/ \+/ /g' | grep "Mem" | cut -d " " -f 4 | grep "Gi" | sed 's/^\([0-9]\+\)Gi$/\1/g' | cut -d "." -f 1)"
	memSize="$(echo "$availableMemory /2" | bc)"

	# Set the output file for the specific run
	runOutputFile="$root_path/data/logs/flipfloyd-$(date +%Y-%m-%d_%H-%M-%S).log"
	# Write information that the experiment is starting
	echo "FlipFloyd Started | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$runOutputFile"
	echo "----------------------------------------------------" >> "$runOutputFile"
	echo "Starting FlipFloyd. Logging output to $runOutputFile" >> "$bitFlipLog"

	# Execute the command with the required parameters
	# * Execute it using `timeout` to limit the runtime. Kill after additional 5 seconds if the program did not stop
	# * Use `stdbuf` to adjust I/O buffering (set to line-buffering, so each line will be processed immediately)
	# * Pipe the output to a loop to process it while the program runs. Note that variables work only limited
  #   within the loop, therefore the "strange" variable handling using files
	# Also, write the exact command executed to the log file for reproducibility
	echo "timeout --kill-after=5 $runtime stdbuf -oL ./rowhammer $memSize 2>&1" >> "$runOutputFile"
	timeout --kill-after=5 $runtime stdbuf -oL ./rowhammer $memSize 2>&1 | while IFS= read -r line;
	do
		# Log all lines of output to the output log and print the output to stdout
		echo "$line"
		echo "$line" >> "$runOutputFile"

		# Count Bit Flips as the tool is running and add it to a special log file
		if [[ "$line" == *"flip at offset"* ]];then
			echo "Bit Flip Found | $(date +%Y-%m-%d_%H-%M-%S)" >> "$bitFlipLog"
			echo "$line" >> "$bitFlipLog"
			echo "-------------------------------------------" >> "$bitFlipLog"
			bitFlips=$(cat "$bitFlipCounter")
			bitFlips=$((bitFlips + 1))
			echo "$bitFlips" > "$bitFlipCounter"
		fi

		# Handle Dumped Core Error
		echo "false" > "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp"
		if [[ "$line" == *"monitored command dumped core"* ]];then
			echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} FlipFloyd Dumped Core. Stopping experiment..."
			echo "true" > "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp"
			killall -9 "rowhammer" > /dev/null 2>&1
		fi

		# Verify that the file size is not exceeded, kill the process if it is and fail the experiment
		fileSize="$(stat "$runOutputFile"  | grep "Size" | cut -d ":" -f 2 | sed 's/^ *\([0-9]\+\) \+.*$/\1/g')"
		if [ "$fileSize" -ge "$FILE_SIZE_LIMIT" ]; then
			echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} FlipFloyd exceeded maximum log file size. Stopping experiment..."
			killall -9 "rowhammer" > /dev/null 2>&1
			exit 1
		fi

	done
	echo "----------------------------------------------------" >> "$runOutputFile"
	echo "FlipFloyd Ended | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$runOutputFile"
	dumped_core=$(cat "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp")
    if [ "$dumped_core" = "true" ];then
        echo  "[$(date +%Y-%m-%d_%H-%M-%S)] | FlipFloyd Dumped Core!" >> "$runOutputFile"
		break
	fi
	runtime=$(($end_timestamp - $(date +%s)))
done

# Change back into the root directory after execution is done
cd "$root_path"

# Count the bit flips and add them to the file
bitFlips=$(cat "$bitFlipCounter")
echo "Total Bit Flips: $bitFlips" >> "$bitFlipLog"

echo "true" > "$root_path/data/tmp/rowhammerToolEnded.tmp"

# Exit the script with correct code
if [ "$dumped_core" = "true" ];then
	exit 1
else
	exit 0
fi
