#!/bin/bash

source conf.cfg

root_path=$ROOT_PATH
lockfile="$root_path/data/tmp/lock.lock"
GlobalDefinesTpl="$root_path/Tools/blacksmith/GlobalDefinesTpl.hpp"
GlobalDefinesFile="$root_path/Tools/blacksmith/include/GlobalDefines.hpp"
thresholdPath="$root_path/data/threshold.txt"
oldThresholdPath="$root_path/data/old-threshold.txt"
tmpHpp="$root_path/data/tmp/tmp_hpp.hpp"
generatedSnippetHpp="$root_path/data/tmp/generated_snippet_blacksmith.hpp"
tmpBool="$root_path/data/tmp/tmp.log"
layer2_loop_bool="$root_path/data/tmp/loop.log"
stdoutConditionPath="$root_path/data/tmp/isStdoutAllowed.log"
layer0_sTimePath="$root_path/data/logs/layer0_sTime.log"
layer1_sTimePath="$root_path/data/logs/layer1_sTime.log"
layer2_sTimePath="$root_path/data/logs/layer2_sTime.log"
stdout="$root_path/Tools/blacksmith/build/stdout.log"
bitFlipCounter="$root_path/data/tmp/blacksmithBitFlipsNum.txt"
buildLogfile="$root_path/data/logs/build.log"
progressStage4="$root_path/data/tmp/stage4.tmp"
all_runs_bitflip_num="$root_path/data/tmp/totalBlacksmithBitFlips.tmp"
echo "0" > "$all_runs_bitflip_num"
SCRIPT_START=$(date +%s)
echo "false" > "$root_path/data/tmp/rowhammerToolEnded.tmp"

TIMEOUT_DURATION_HAMMERTOOL=$(cat "$root_path/data/tmp/available_runtime.tmp")

loop="true"
echo "false" > "$stdoutConditionPath"
ranks=$(cat "$root_path/data/logs/blacksmithRanks-1.log")
stdCond="true"
oldThreshold=$(cat "$thresholdPath")
echo "$oldThreshold" > "$oldThresholdPath"
echo $(date +%s) > "$layer2_sTimePath"

start_marker="/* START_DYNAMIC_INJECTION */"
end_marker="/* END_DYNAMIC_INJECTION */"

# acquire_lock() {
#     exec 200>"$lockfile"
#     flock -n 200 || { echo "Unable to acquire lock"; exit 1; }
# }

# release_lock() {
#     flock -u 200
#     rm -f "$lockfile"
# }

round_to_nearest_10() {
  local value=$1
  mod=$(echo "$value % 10" | bc)

  comparison=$(echo "$mod < 5" | bc)
  if [ "$comparison" -eq 1 ]; then
    rounded=$(echo "$value - $mod" | bc)
  else
    rounded=$(echo "$value + (10 - $mod)" | bc)
  fi
  echo $rounded
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

runtime_handler() {
    local layerX_sTimePath="$1"
    local timeout="$2"
    local layerX_sTime=$(cat "$layerX_sTimePath")
    while [ -z "$layerX_sTime" ]
    do
        layerX_sTime=$(cat "$layerX_sTimePath")
    done
    local now=$(date +%s)
    local diff=$((now - layerX_sTime))
    if [ "$diff" -gt "$timeout" ];then
        return 1
    else
        return 0
    fi
}

script_timeout_handler() {
    local script_timeout="$1"
    local layer2_sTime=$(cat "$layer2_sTimePath")
    local now=$(date +%s)
    local elapsed_script_runtime=$((now - layer2_sTime))
    if [ "$elapsed_script_runtime" -lt "0" ];then
        elapsed_script_runtime="0"
    fi
    local new_script_timeout=$((script_timeout - elapsed_script_runtime))
    echo "$new_script_timeout"
}

diff=$(echo "$oldThreshold * 0.25" | bc)
min=$(echo "$oldThreshold - $diff" | bc)
max=$(echo "$oldThreshold + $diff" | bc)
min_rounded=$(round_to_nearest_10 $min)
min_rounded=${min_rounded%.*}
max_rounded=$(round_to_nearest_10 $max)
max_rounded=${max_rounded%.*}
newThreshold="$min_rounded"

# A defined default threshold for rare cases | Default threshold is: 520
max_rounded_def="880"
newThreshold_def="520"

# --------------------- First Initialisation of the outputs | START -------------------------------------
dateForTitle=$(date +%Y-%m-%d_%H-%M-%S)
bitFlipReport="$root_path/data/bit-flip-reports-blacksmith.txt"
stdoutFinalPath="$root_path/data/logs/blacksmith-$dateForTitle.log"
echo "Blacksmith Started | [$dateForTitle] | timestamp: $(date +%s)" > "$stdoutFinalPath"
echo "----------------------------------------------------" >> "$stdoutFinalPath"
# --------------------- First Initialisation of the outputs | END -------------------------------------

while [ $loop = "true" ]
do
    stdoutCondition=$(cat "$stdoutConditionPath")
    layer1_termination_triggered="false"
    script_timeout=$(script_timeout_handler "$TIMEOUT_DURATION_HAMMERTOOL")
    scriptExited="false"
    bash "$root_path/components/utils/hugepage.sh"
    echo "false" > "$tmpBool"

    if [ "$layer1_termination_triggered" = "false" ];then
        > "$tmpHpp"
        echo -e "${BLACK_TXT}${PURPLE_BG}  Rowhammer ${COLOR_RESET} Building Blacksmith ..."
        echo "Start building blacksmith | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogfile"
        cd "$root_path/Tools/blacksmith/"
        mkdir -p build
        cd build
        cmake ../
        make -j$(nproc) >> "$buildLogfile" 2>&1
        echo -e "${BLACK_TXT}${PURPLE_BG}  Rowhammer ${COLOR_RESET} Building Blacksmith done!"
        echo "Done building blacksmith | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogfile"
    fi

    echo -e "${BLACK_TXT}${PURPLE_BG}  Rowhammer ${COLOR_RESET} Initiating Blacksmith Now ..."

    timeout=900  # Timeout for exiting infinite loops in seconds
    while true;do
        stdoutCondition=$(cat "$stdoutConditionPath")
        script_timeout=$(script_timeout_handler "$TIMEOUT_DURATION_HAMMERTOOL")
        > "$stdout"
        if [ "$stdoutCondition" = "true" ];then
            dateForTitle=$(date +%Y-%m-%d_%H-%M-%S)
            stdoutFinalPath="$root_path/data/logs/blacksmith-$dateForTitle.log"
            echo "Blacksmith Started | [$dateForTitle] | timestamp: $(date +%s)" > "$stdoutFinalPath"
            echo "----------------------------------------------------" >> "$stdoutFinalPath"
            echo "Starting Blacksmith. Logging output to $stdoutFinalPath" >> "$bitFlipReport"
        fi
        echo "0" > "$bitFlipCounter"

        execute_command() {
            stdbuf -oL "$root_path/Tools/blacksmith/build/blacksmith" --dimm-id 1 --runtime-limit $script_timeout --ranks $ranks --sweeping |
            while IFS= read -r line
            do
                echo $(date +%s) > "$layer0_sTimePath"
                echo "$line"
                echo "$line" >> "$stdoutFinalPath"
                if [[ "$line" == *"Bit Flip Found!"* ]];then
                    echo "Bit Flip Found | $(date +%Y-%m-%d_%H-%M-%S)" >> "$bitFlipReport"
                    echo "$line" >> "$bitFlipReport"
                    echo "-------------------------------------------" >> "$bitFlipReport"
                    bitFlips=$(cat "$bitFlipCounter")
                    bitFlips=$((bitFlips + 1))
                    echo "$bitFlips" > "$bitFlipCounter"
                fi
                if [[ "$line" == *"Initializing memory with pseudorandom sequence."* || "$line" == *"Could not find conflicting address sets"* ||
                "$line" == *"Found bank conflicts."* || "$line" == *"Bank/rank functions"* || "$line" == *"PAGE_SIZE"* || -z "$line" ]]; then
                    echo "true" > "$layer2_loop_bool"
                    echo "false" > "$stdoutConditionPath"
                else
                    echo "false" > "$layer2_loop_bool"
                    echo "true" > "$stdoutConditionPath"
                fi
            done
        }

        execute_command & command_pid=$!

        if ps -p $command_pid > /dev/null; then
            while true;do
                sleep 1
                # --------------------- Handle Layer 2 Timeout ---------------------
                runtime_handler "$layer2_sTimePath" "$script_timeout"
                l2Status=$?
                if [ "$l2Status" = "1" ]; then
                    killall -9 blacksmith > /dev/null 2>&1
                    layer2_timeout_triggered="true"
                    break
                else
                    layer2_timeout_triggered="false"
                fi
                # --------------------- Handle Layer 2 Timeout End ---------------------
                
                # --------------------- Handle Layer 0 Timeout ---------------------
                runtime_handler "$layer0_sTimePath" "$timeout"
                l0Status=$?
                if [ "$l0Status" = "1" ]; then
                    killall -9 blacksmith > /dev/null 2>&1
                    layer0_timeout_triggered="true"
                    break
                else
                    layer0_timeout_triggered="false"
                fi
                # --------------------- Handle Layer 0 Timeout End ---------------------
            done
        fi
        # ----------------------- Break out of the layer 1 loop -----------------------
        if [ "$layer2_timeout_triggered" = "true" ];then
            break
        fi
        # ----------------------- Break out of the layer 1 loop End -----------------------

        # --------------------- Handle Layer 1 Timeout ---------------------
        loop=$(cat "$layer2_loop_bool")
        script_timeout=$(script_timeout_handler "$TIMEOUT_DURATION_HAMMERTOOL")
        if [[ ( "$layer0_timeout_triggered" == "true" && "$loop" == "false" ) || ( "$script_timeout" -gt "600" && "$loop" == "false" ) ]];then
            layer1_termination_triggered="true"
            echo -e "${BLACK_TXT}${PURPLE_BG}  Rowhammer ${COLOR_RESET} ${GREEN_TXT}Blacksmith is done! | [$(date +%Y-%m-%d_%H-%M-%S)]${COLOR_RESET}"
            bitFlips=$(cat "$bitFlipCounter")
            tbf=$(cat "$all_runs_bitflip_num")
            tbf=$((tbf + bitFlips))
            echo "$tbf" > "$all_runs_bitflip_num"
            echo "----------------------------------------------------" >> "$stdoutFinalPath"
            echo "Blacksmith Ended | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$stdoutFinalPath"
            echo "pidLocal Var=$pidLocal | " >> "$stdoutFinalPath"
            echo "command_pid Var=$command_pid | " >> "$stdoutFinalPath"
            echo "layer0_timeout_triggered=$layer0_timeout_triggered | " >> "$stdoutFinalPath"
            echo "layer1_termination_triggered=$layer1_termination_triggered | " >> "$stdoutFinalPath"
            echo "layer2_timeout_triggered=$layer2_timeout_triggered | " >> "$stdoutFinalPath"
            echo -e "${BLACK_TXT}${PURPLE_BG}  Rowhammer ${COLOR_RESET} ${RED_TXT}Blacksmith layer 1 timeout reached!${COLOR_RESET} Re-Running Blacksmith with shorter runtime ..."
        else
            break
        fi
        # --------------------- Handle Layer 1 Timeout End ---------------------
    done

    # ----------------------- Break out of the layer 2 loop -----------------------
    if [ "$layer2_timeout_triggered" = "true" ];then
        scriptExited="true"
        break
    fi
    # ----------------------- Break out of the layer 2 loop End -----------------------

    if [ "$newThreshold" = "$max_rounded" ];then
        max_rounded="99999"
        newThreshold="$newThreshold_def"
    fi
    if [ "$newThreshold" = "$max_rounded_def" ];then
        scriptExited="true"
        break
    fi

    loop=$(cat "$layer2_loop_bool")
    if [ "$loop" = "true" ];then
        echo -e "${BLACK_TXT}${PURPLE_BG}  Rowhammer ${COLOR_RESET} ${RED_TXT}Blacksmith failed!${COLOR_RESET}"
    else
        scriptExited="true"
        break
    fi

    if [ "$layer1_termination_triggered" = "false" ];then
        newThreshold=$((newThreshold + 10))
        echo "$newThreshold" > "$thresholdPath"
        echo -e "${BLACK_TXT}${PURPLE_BG}  Rowhammer ${COLOR_RESET} Re-Injecting new Threshold to Blacksmith ..."
        python3 "$root_path/components/rowhammer/buildBlacksmith.py" -t
        
        while IFS= read -r line
        do
            if [[ "$line" == "$start_marker" ]]; then
                echo "true" > "$tmpBool"
                echo "$line" >> "$tmpHpp"
                cat "$generatedSnippetHpp" >> "$tmpHpp"
            elif [[ "$line" == "$end_marker" ]]; then
                echo "false" > "$tmpBool"
            fi

            cond=$(cat "$tmpBool")

            if [[ "$cond" == false ]]; then
                if [[ "$line" == "$end_marker" ]]; then
                    echo -e "\n$line" >> "$tmpHpp"
                else
                    echo "$line" >> "$tmpHpp"
                fi
            fi
        done < "$GlobalDefinesTpl"
        mv "$tmpHpp" "$GlobalDefinesFile"
        echo -e "${BLACK_TXT}${PURPLE_BG}  Rowhammer ${COLOR_RESET} New Threshold of $newThreshold injected to Blacksmith."
    fi
done

echo "true" > "$root_path/data/tmp/rowhammerToolEnded.tmp"

echo -e "${BLACK_TXT}${PURPLE_BG}  Rowhammer ${COLOR_RESET} ${GREEN_TXT}Blacksmith is done! | [$(date +%Y-%m-%d_%H-%M-%S)]${COLOR_RESET}"
bitFlips=$(cat "$bitFlipCounter")
tbf=$(cat "$all_runs_bitflip_num")
tbf=$((tbf + bitFlips))
echo "Total Bit Flips: $tbf" >> "$bitFlipReport"
echo "----------------------------------------------------" >> "$stdoutFinalPath"
echo "Blacksmith Ended | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$stdoutFinalPath"
echo "scriptExited=$scriptExited | " >> "$stdoutFinalPath"
echo "pidLocal Var=$pidLocal | " >> "$stdoutFinalPath"
echo "command_pid Var=$command_pid | " >> "$stdoutFinalPath"
echo "layer0_timeout_triggered=$layer0_timeout_triggered | " >> "$stdoutFinalPath"
echo "layer1_termination_triggered=$layer1_termination_triggered | " >> "$stdoutFinalPath"
echo "layer2_timeout_triggered=$layer2_timeout_triggered | " >> "$stdoutFinalPath"
