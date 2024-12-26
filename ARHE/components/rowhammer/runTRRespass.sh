#!/bin/bash

source conf.cfg

root_path=$ROOT_PATH
bitFlipReport="$root_path/data/bit-flip-reports-hammersuite.txt"
def_fns_path="$root_path/data/hammersuiteDefFns.txt"
bitFlipCounter="$root_path/data/tmp/hammersuiteBitFlipsNum.tmp"
errPath="$root_path/data/errors"
buildLogfile="$root_path/data/logs/build.log"
TIMEOUT_DURATION_HAMMERTOOL=$(cat "$root_path/data/tmp/available_runtime.tmp")
progressStage4="$root_path/data/tmp/stage4.tmp"
echo "false" > "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp"
echo "false" > "$root_path/data/tmp/rowhammerToolEnded.tmp"
max_size=1073741824
timeout_reached=false
SCRIPT_START=$(date +%s)
END_TIMESTAMP=$(( SCRIPT_START + TIMEOUT_DURATION_HAMMERTOOL ))

echo "true" > "$root_path/data/tmp/hammersuiteStatus.tmp"
echo "false" > "$root_path/data/tmp/hammersuiteTimeoutReached.tmp"
echo "false" > "$def_fns_path"
def_fns=$(cat "$def_fns_path")

get_file_size() {
    local file="$1"
    if [ -f "$file" ]; then
        stat -c%s "$file"
    else
        echo 0
    fi
}

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

finalOutput="$root_path/data/logs/hammersuite-$(date +%Y-%m-%d_%H-%M-%S).log"
echo "0" > "$bitFlipCounter"

echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} Building TRRespass ..."
cd "$root_path/Tools/hammersuite/"
echo "Start building TRRespass | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogfile"
make >> "$buildLogfile" 2>&1
if [ $? -ne 0 ]; then
    echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} Building TRRespass failed!"
    echo "Failed building TRRespass | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogfile"
    exit 1
else
    echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} Building TRRespass done!"
    echo "Done building TRRespass | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogfile"
fi

bash "$root_path/components/utils/hugepage.sh"

runtime=$(($END_TIMESTAMP - $(date +%s)))
while [[ $runtime -gt 60 && "$def_fns" == "false" ]]
do
    finalOutput="$root_path/data/logs/hammersuite-$(date +%Y-%m-%d_%H-%M-%S).log"
    echo "TRRespass Started | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$finalOutput"
    echo "----------------------------------------------------" >> "$finalOutput"
    echo "Starting TRRespass. Logging output to $finalOutput" >> "$bitFlipReport"
    timeout --kill-after=5 $runtime stdbuf -oL "$root_path/Tools/hammersuite/obj/tester" -v "fuzzing" 2>&1 | while IFS= read -r line;
    do
        echo "$line"
        initialOutputCSV=$(find "$root_path/Tools/hammersuite/data" -maxdepth 1 -type f -name "*.csv")
        current_size=$(get_file_size "$initialOutputCSV")
        echo "$line" >> "$finalOutput"
        if [[ "$line" == *"Flip"* ]];then
            echo "Bit Flip Found | $(date +%Y-%m-%d_%H-%M-%S)" >> "$bitFlipReport"
            echo "$line" >> "$bitFlipReport"
            echo "-------------------------------------------" >> "$bitFlipReport"
            bitFlips=$(cat "$bitFlipCounter")
            bitFlips=$((bitFlips + 1))
            echo "$bitFlips" > "$bitFlipCounter"
        fi
        if [ "$current_size" -ge "$max_size" ]; then
            echo  "[$(date +%Y-%m-%d_%H-%M-%S)] | CSV file limit reached for injected functions!" >> "$errPath/hammersuite.log"
            echo "true" > "$def_fns_path"
            pid=$(ps aux | grep "tester -v fuzzing" | grep -v grep | awk '$8 ~ /R/ {print $2}')
            if [ -n "$pid" ];then
                kill -9 $pid > /dev/null 2>&1
            fi
            killall "tester" > /dev/null 2>&1
            break
        fi

        if [[ "$line" == *"Mapping function"* && "$line" == *"not respected"* ]]; then
            echo  "[$(date +%Y-%m-%d_%H-%M-%S)] | Mapping functions with the injected functions are not respected!" >> "$errPath/hammersuite.log"
            echo "true" > "$def_fns_path"
            pid=$(ps aux | grep "tester -v fuzzing" | grep -v grep | awk '$8 ~ /R/ {print $2}')
            if [ -n "$pid" ];then
                kill -9 $pid > /dev/null 2>&1
            fi
            killall "tester" > /dev/null 2>&1
            break
        fi
        # Handle Dumped Core Error
		echo "false" > "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp"
		if [[ "$line" == *"monitored command dumped core"* ]];then
			echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} TRRespass Dumped Core. Stopping experiment..."
			echo "true" > "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp"
			killall -9 "tester" > /dev/null 2>&1
		fi
    done
    echo "----------------------------------------------------" >> "$finalOutput"
    echo "TRRespass Ended | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$finalOutput"
    def_fns=$(cat "$def_fns_path")
    dumped_core=$(cat "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp")
	if [ "$dumped_core" = "true" ];then
        echo "false" > "$root_path/data/tmp/hammersuiteStatus.tmp"
        echo  "[$(date +%Y-%m-%d_%H-%M-%S)] | TRRespass Dumped Core!" >> "$finalOutput"
		break
	fi
    runtime=$(($END_TIMESTAMP - $(date +%s)))
done


finalOutputCSV="$root_path/data/logs/hammersuiteCSV-$(date +%Y-%m-%d_%H-%M-%S).csv"
initialOutputCSV=$(find "$root_path/Tools/hammersuite/data" -maxdepth 1 -type f -name "*.csv")
mv "$initialOutputCSV" "$finalOutputCSV"

cd "$root_path"

def_fns=$(cat "$def_fns_path")


if [[ "$def_fns" == "true" ]]; then
    echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} ${RED_TXT}Initial functions were NOT accepted!${COLOR_RESET} Switching to default functions ..."
    bash "$root_path/components/rowhammer/buildTRRespass.sh"
    echo "0" > "$bitFlipCounter"

    echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} Building TRRespass ..."
    cd "$root_path/Tools/hammersuite/"
    echo "Start building TRRespass | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogfile"
    make >> "$buildLogfile" 2>&1
    if [ $? -ne 0 ]; then
        echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} Building TRRespass failed!"
        echo "Failed building TRRespass | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogfile"
        exit 1
    else
        echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} Building TRRespass done!"
        echo "Done building TRRespass | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogfile"
    fi

    runtime=$(($END_TIMESTAMP - $(date +%s)))
    while [[ $runtime -gt 60 ]]
    do
        finalOutput="$root_path/data/logs/hammersuiteDefFns-$(date +%Y-%m-%d_%H-%M-%S).log"
        echo "TRRespass Started | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$finalOutput"
        echo "----------------------------------------------------" >> "$finalOutput"
        echo "Starting TRRespass. Logging output to $finalOutput" >> "$bitFlipReport"
        timeout --kill-after=5 $TIMEOUT_DURATION_HAMMERTOOL stdbuf -oL "$root_path/Tools/hammersuite/obj/tester" -v "fuzzing" 2>&1 | while IFS= read -r line;
        do
            echo "$line"
            initialOutputCSV=$(find "$root_path/Tools/hammersuite/data" -maxdepth 1 -type f -name "*.csv")
            current_size=$(get_file_size "$initialOutputCSV")
            echo "$line" >> "$finalOutput"
            if [[ "$line" == *"Flip"* ]];then
                echo "Bit Flip Found | $(date +%Y-%m-%d_%H-%M-%S)" >> "$bitFlipReport"
                echo "$line" >> "$bitFlipReport"
                echo "-------------------------------------------" >> "$bitFlipReport"
                bitFlips=$(cat "$bitFlipCounter")
                bitFlips=$((bitFlips + 1))
                echo "$bitFlips" > "$bitFlipCounter"
            fi
            if [ "$current_size" -ge "$max_size" ]; then
                echo  "[$(date +%Y-%m-%d_%H-%M-%S)] | CSV file limit reached for default functions!" >> "$errPath/hammersuite.log"
                echo "false" > "$root_path/data/tmp/hammersuiteStatus.tmp"
                pid=$(ps aux | grep "tester -v fuzzing" | grep -v grep | awk '$8 ~ /R/ {print $2}')
                if [ -n "$pid" ];then
                    kill -9 $pid > /dev/null 2>&1
                fi
                killall "tester" > /dev/null 2>&1
                break
            fi

            # Handle Dumped Core Error
            echo "false" > "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp"
            if [[ "$line" == *"monitored command dumped core"* ]];then
                echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} TRRespass Dumped Core. Stopping experiment..."
                echo "true" > "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp"
                killall -9 "tester" > /dev/null 2>&1
            fi

            if [[ "$line" == *"Mapping function"* && "$line" == *"not respected"* ]]; then
                echo  "[$(date +%Y-%m-%d_%H-%M-%S)] | Mapping functions with the default functions are not respected!" >> "$errPath/hammersuite.log"
                echo "false" > "$root_path/data/tmp/hammersuiteStatus.tmp"
                pid=$(ps aux | grep "tester -v fuzzing" | grep -v grep | awk '$8 ~ /R/ {print $2}')
                if [ -n "$pid" ];then
                    kill -9 $pid > /dev/null 2>&1
                fi
                killall "tester" > /dev/null 2>&1
                break
            fi
        done
    echo "----------------------------------------------------" >> "$finalOutput"
    echo "TRRespass Ended | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$finalOutput"
    def_fns=$(cat "$def_fns_path")
    dumped_core=$(cat "$root_path/data/tmp/rowhammerTool_Dumped_core.tmp")
	if [ "$dumped_core" = "true" ];then
        echo "false" > "$root_path/data/tmp/hammersuiteStatus.tmp"
        echo  "[$(date +%Y-%m-%d_%H-%M-%S)] | TRRespass Dumped Core!" >> "$finalOutput"
		break
	fi
    runtime=$(($END_TIMESTAMP - $(date +%s)))
    done
fi

finalOutputCSV="$root_path/data/logs/hammersuiteDefFnsCSV-$(date +%Y-%m-%d_%H-%M-%S).csv"
initialOutputCSV=$(find "$root_path/Tools/hammersuite/data" -maxdepth 1 -type f -name "*.csv")
if [ -n "$initialOutputCSV" ];then
    mv "$initialOutputCSV" "$finalOutputCSV"
fi
bitFlips=$(cat "$bitFlipCounter")
echo "Total Bit Flips: $bitFlips" >> "$bitFlipReport"

cd "$root_path"

echo "true" > "$root_path/data/tmp/rowhammerToolEnded.tmp"

hammersuiteStatus=$(cat "$root_path/data/tmp/hammersuiteStatus.tmp")
if [[ "$hammersuiteStatus" == "false" ]]; then
    echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} ${RED_TXT}TRRespass failed! | [$(date +%Y-%m-%d_%H-%M-%S)]${COLOR_RESET}"
    exit 1
else
    echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} ${GREEN_TXT}TRRespass is successfully done! | [$(date +%Y-%m-%d_%H-%M-%S)]${COLOR_RESET}"
fi
