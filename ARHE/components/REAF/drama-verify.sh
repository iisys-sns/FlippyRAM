#!/bin/bash

source conf.cfg

root_path=$ROOT_PATH
errPath="$root_path/data/errors"
log_file="$root_path/data/logs/drama-verify.log"
address_file="$root_path/data/reaf-pool.txt"
output_file="$root_path/data/reaf-pool-verified.txt"
memClass=$(cat "$root_path/data/sys-info/class.txt")
sTimePath="$root_path/data/tmp/stime.log"
loopPath="$root_path/data/tmp/dv-loop.txt"
progress="$root_path/data/tmp/stage3.tmp"
echo "true" > "$loopPath"
timeout=300

echo "DRAMA-Verify Started | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" > "$log_file"
echo "----------------------------------------------------" >> "$log_file"

batch_size=$(cat "$root_path/data/functions-num.txt")

echo -e "-------------$(date +%Y-%m-%d_%H-%M-%S)-------------\n" > "$output_file"
echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Initiating Drama-Verify ..."

echo "10%" > "$progress"

# pad_array=(0x0 0x1 0x2 0x3 0x4 0x5 0x6) # Not used anymore, in the new ARHE, the padding
# is handled by adding addresess from the start of the array list making the drama-verify tool
# work better and faster | This array was used for padding so that Drama-Verify does not enter an infinite loop - Now handled differently
counter=1
success="false"
> "$output_file"
while [[ "$counter" < 4 ]] 
do
    loop=$(cat "$loopPath")
    IFS=',' read -r -a addresses < "$address_file"
    total_addresses=${#addresses[@]}
    for (( i=0; i<total_addresses; i+=batch_size )); do
        echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Verifying a set ..."
        chunk=("${addresses[@]:i:batch_size}")
        if (( ${#chunk[@]} < batch_size )); then
            pad_needed=$((batch_size - ${#chunk[@]}))
            chunk+=( "${addresses[@]:0:pad_needed}" )
        fi
        chunk_str=$(IFS=,; echo "${chunk[*]}")
        # Prepare flags based on memory class
        if [[ "$memClass" == "DDR3" ]]; then
            flags="--memory-type=ddr3 --linear-addressing-functions $chunk_str"
        else
            flags="--linear-addressing-functions $chunk_str"
        fi
        timeout $timeout stdbuf -oL "$root_path/Tools/Drama-Verify/bin/drama-verify" $flags | while IFS= read -r line;
        do
            echo $(date +%s) > "$sTimePath"
            echo "$line" >> "$log_file"
            if [[ "$line" == *"difference of"* ]]; then
                echo "$line"
                address_exists=false
                if [[ $line =~ Function[[:space:]](0x[0-9a-fA-F]+) ]]; then
                    hex_value="${BASH_REMATCH[1]}"
                fi
                if grep -Fwq "$hex_value" "$output_file"; then
                    address_exists=true
                fi
                # Only append the line if none of the addresses were found in the output_file
                if [ "$address_exists" = false ]; then
                    echo "$line" >> "$output_file"
                fi
                echo "false" > "$loopPath"
            fi
        done
        # ---- Second timeout control just to prevent any bugs ----
        now=$(date +%s)
        sTime=$(cat "$sTimePath")
        elapsedTime=$((now - sTime))
        elapsedTime=$((elapsedTime + 10))
        loop=$(cat "$loopPath")
        if [[ "$elapsedTime" > "$timeout" ]] || [[ "$loop" == "true" ]]; then
            echo "true" > "$loopPath"
            echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} ${RED_TXT}Verifying Failed! Trying Again ...${COLOR_RESET}"
            echo "[$(date +%Y-%m-%d_%H-%M-%S)] | $chunk_str | Try $counter Failed!" >> "$errPath/drama-verify.log"
            break
        else
            echo "false" > "$loopPath"
            echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Verifying of one set of functions is done!"
            killall drama-verify > /dev/null 2>&1
        fi
    done
    loop=$(cat "$loopPath")
    if [ "$loop" = "false" ];then
        success="true"
        break
    fi
    counter=$((counter + 1))
done

echo "----------------------------------------------------" >> "$log_file"
echo "DRAMA-Verify Ended | [$(date +%Y-%m-%d_%H-%M-%S)] | timestamp: $(date +%s)" >> "$log_file"

if [ "$success" = "false" ];then
    echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} ${RED_TXT}Drama-Verify Failed But Some Functions might've been stored!${COLOR_RESET}"
    echo "[$(date +%Y-%m-%d_%H-%M-%S)] | Drama-Verify Failed!" >> "$errPath/drama-verify.log"
else
    echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} ${GREEN_TXT}All addressing functions are verified now!${COLOR_RESET}"
fi

echo "70%" > "$progress"
