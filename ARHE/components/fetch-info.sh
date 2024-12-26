#!/bin/bash

source conf.cfg

root_path=$ROOT_PATH
dir="$root_path/data/"
mkdir -p "$dir/sys-info"
dataDir="$root_path/data/sys-info"
errPath="$root_path/data/errors"
progress="$root_path/data/tmp/stage1.tmp"

dmidecode -t bios > "$dataDir/dmidecode_bios.txt" && echo -e "${BLUE_BG} Fetch ${COLOR_RESET} bios information stored in dmidecode_bios.txt - ${GREEN_TXT}SUCCESS${COLOR_RESET}" || echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: dmidecode -t bios - FAILED"

dmidecode -t system > "$dataDir/dmidecode_system.txt" && echo -e "${BLUE_BG} Fetch ${COLOR_RESET} system information stored in dmidecode_system.txt - ${GREEN_TXT}SUCCESS${COLOR_RESET}" || echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: dmidecode -t system - FAILED"

dmidecode -t baseboard > "$dataDir/dmidecode_baseboard.txt" && echo -e "${BLUE_BG} Fetch ${COLOR_RESET} baseboard information stored in dmidecode_baseboard.txt - ${GREEN_TXT}SUCCESS${COLOR_RESET}" || echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: dmidecode -t baseboard - FAILED"

dmidecode -t chassis > "$dataDir/dmidecode_chassis.txt" && echo -e "${BLUE_BG} Fetch ${COLOR_RESET} chassis information stored in dmidecode_chassis.txt - ${GREEN_TXT}SUCCESS${COLOR_RESET}" || echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: dmidecode -t chassis - FAILED"

dmidecode -t processor > "$dataDir/dmidecode_processor.txt" && echo -e "${BLUE_BG} Fetch ${COLOR_RESET} processor information stored in dmidecode_processor.txt - ${GREEN_TXT}SUCCESS${COLOR_RESET}" || echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: dmidecode -t processor - FAILED"

dmidecode -t memory > "$dataDir/dmidecode_memory.txt" && echo -e "${BLUE_BG} Fetch ${COLOR_RESET} memory information stored in dmidecode_memory.txt - ${GREEN_TXT}SUCCESS${COLOR_RESET}" || echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: dmidecode -t memory - FAILED"

dmidecode -t cache > "$dataDir/dmidecode_cache.txt" && echo -e "${BLUE_BG} Fetch ${COLOR_RESET} cache information stored in dmidecode_cache.txt - ${GREEN_TXT}SUCCESS${COLOR_RESET}" || echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: dmidecode -t cache - FAILED"

dmidecode -t connector > "$dataDir/dmidecode_connector.txt" && echo -e "${BLUE_BG} Fetch ${COLOR_RESET} connector information stored in dmidecode_connector.txt - ${GREEN_TXT}SUCCESS${COLOR_RESET}" || echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: dmidecode -t connector - FAILED"

dmidecode -t slot > "$dataDir/dmidecode_slot.txt" && echo -e "${BLUE_BG} Fetch ${COLOR_RESET} slot information stored in dmidecode_slot.txt - ${GREEN_TXT}SUCCESS${COLOR_RESET}" || echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: dmidecode -t slot - FAILED"

lscpu > "$dataDir/lscpu.txt" && echo -e "${BLUE_BG} Fetch ${COLOR_RESET} cpu information stored in lscpu.txt - ${GREEN_TXT}SUCCESS${COLOR_RESET}" || echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: lscpu - FAILED"

dmesg | grep microcode > "$dataDir/microcode.txt" && echo -e "${BLUE_BG} Fetch ${COLOR_RESET} microcode information stored in microcode.txt - ${GREEN_TXT}SUCCESS${COLOR_RESET}" || echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: dmesg - FAILED"

bash "$root_path/components/utils/decode-dimms.sh" > "$dataDir/decode-dimms.txt" 2>&1 && echo -e "${BLUE_BG} Fetch ${COLOR_RESET} decode-dimms information stored in decode-dimms.txt - ${GREEN_TXT}SUCCESS${COLOR_RESET}" || echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: decode-dimms - FAILED"
ddm=$(cat "$dataDir/decode-dimms.txt")
if [ -z "$ddm" ];then
    bash "$root_path/components/utils/decode-dimms.sh" -n > "$dataDir/decode-dimms.txt" 2>&1 && echo -e "${BLUE_BG} Fetch ${COLOR_RESET} decode-dimms information stored in decode-dimms.txt - ${GREEN_TXT}SUCCESS${COLOR_RESET}" || echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: decode-dimms - FAILED"
fi

echo "33%" > "$progress"

# ------------------------------------- Important Variables -------------------------------------
dd=$(cat "$dataDir/dmidecode_memory.txt") # ----------------> dmidecode -t memory output
ddm=$(cat "$dataDir/decode-dimms.txt") # -------------------> decode-dimms.sh output

# ------------------------------------- Memory/s Class Detection -------------------------------------
memClass=$(echo "$dd" | grep "DDR" | head -n 1 | awk '{print $2}')
if [ -z "$memClass" ];then
    memClass=$(echo "$ddm" | grep "DDR" | head -n 1 | awk '{print $4}')
fi
if [ -z "$memClass" ];then
    memClass="DDR4" # TODO: get info from other ways if possible
else
    echo "$memClass" > "$dataDir/class.txt"
fi
echo "$memClass" > "$dataDir/class.txt"
# ------------------------------------- Memory/s Class End -------------------------------------


# ------------------------------------- Refresh Rate Detection -------------------------------------
if [[ "$memClass" == "DDR3" ]];then
    output=$("$root_path/Tools/measureRefreshRate/bin/measureRefreshRate" -g ddr3 2>&1)
    rc=$?
else
    output=$("$root_path/Tools/measureRefreshRate/bin/measureRefreshRate" 2>&1)
    rc=$?
fi

if [ "$rc" != "0" ]; then
    echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: Refresh Rate - FAILED"
else
    echo "$output" > "$dataDir/refresh_rate.txt" \
        && echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Refresh Rate information stored in refresh_rate.txt - ${GREEN_TXT}SUCCESS${COLOR_RESET}" \
        || echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Error: Refresh Rate - FAILED"
fi
# ------------------------------------- Refresh Rate Detection End -------------------------------------

# ------------------------------------- Number of DIMMs Detection -------------------------------------
dimmNum=$(echo "$ddm" | grep "Number of SDRAM DIMMs detected and decoded" | awk '{print $8}')
dimmNumDmiDecode=$(echo "$dd" | grep "Total Width:" | grep -v -c "Unknown")
if [[ -z "$dimmNum" || "$dimmNum" == "0" ]];then
    dimmNum="$dimmNumDmiDecode"
    echo "true" > "$root_path/data/isDimmNumCorrect.txt"
else
    if [ "$dimmNumDmiDecode" -gt "$dimmNum" ];then
        echo "false" > "$root_path/data/isDimmNumCorrect.txt"
    else
        echo "true" > "$root_path/data/isDimmNumCorrect.txt"
    fi
fi
if [ -z "$dimmNum" ];then
    dimmNum=1 # Last Resort
fi
echo "$dimmNum" > "$dataDir/dimm-numbers.txt"
# ------------------------------------- Number of DIMMs Detection End -------------------------------------

# ------------------------------------- AMD Detection -------------------------------------
isAmd=$(cat /proc/cpuinfo | grep -i "amd" | tr -d '[:space:]')
if [ -z "$isAmd" ];then
    echo "false" > "$dir/isAmd.txt"
else
    echo "true" > "$dir/isAmd.txt"
fi
# ------------------------------------- AMD Detection End -------------------------------------

echo "66%" > "$progress"

# ------------------------------------- Docker Detection -------------------------------------
isDocker="false"
if [ -f "/.dockerenv" ];then
    isDocker="true"
fi
echo $isDocker > "$dataDir/isDocker.txt"
# ------------------------------------- Docker Detection End -------------------------------------

# ------------------------------------- Channel Detection -------------------------------------
echo "0" > "$dataDir/channels.txt"
echo "false" > "$root_path/data/tmp/chDetected.tmp"
echo "false" > "$root_path/data/tmp/chA.tmp"
echo "false" > "$root_path/data/tmp/chB.tmp"

echo "$dd" | while IFS= read -r line; do
channels=$(cat "$dataDir/channels.txt")
    if [[ "$line" == *"Size"* ]];then
        if [[ "${line,,}" == *"gb"* ]];then
            echo "true" > "$root_path/data/tmp/chDetected.tmp"
        else
            echo "false" > "$root_path/data/tmp/chDetected.tmp"
        fi
    fi
channelDetected=$(cat "$root_path/data/tmp/chDetected.tmp")
if [[ "$channelDetected" == "true" && "$line" == *"Locator"* ]];then
    channelAexhausted=$(cat "$root_path/data/tmp/chA.tmp")
    channelBexhausted=$(cat "$root_path/data/tmp/chB.tmp")
    if [[ "${line,,}" == *"channela"* && "$channelAexhausted" == "false" ]];then
        echo "false" > "$root_path/data/tmp/chDetected.tmp"
        echo "true" > "$root_path/data/tmp/chA.tmp"
        channels=$((channels + 1))
        echo "$channels" > "$dataDir/channels.txt"
    elif [[ "${line,,}" == *"channelb"* && "$channelBexhausted" == "false" ]];then
        echo "false" > "$root_path/data/tmp/chDetected.tmp"
        echo "true" > "$root_path/data/tmp/chB.tmp"
        channels=$((channels + 1))
        echo "$channels" > "$dataDir/channels.txt"
    elif [[ "${line,,}" == *"channel a"* && "$channelAexhausted" == "false" ]];then
        echo "false" > "$root_path/data/tmp/chDetected.tmp"
        echo "true" > "$root_path/data/tmp/chA.tmp"
        channels=$((channels + 1))
        echo "$channels" > "$dataDir/channels.txt"
    elif [[ "${line,,}" == *"channel b"* && "$channelBexhausted" == "false" ]];then
        echo "false" > "$root_path/data/tmp/chDetected.tmp"
        echo "true" > "$root_path/data/tmp/chB.tmp"
        channels=$((channels + 1))
        echo "$channels" > "$dataDir/channels.txt"
    elif [[ "${line,,}" == *"dimm a"* && "$channelAexhausted" == "false" ]];then
        echo "false" > "$root_path/data/tmp/chDetected.tmp"
        echo "true" > "$root_path/data/tmp/chA.tmp"
        channels=$((channels + 1))
        echo "$channels" > "$dataDir/channels.txt"
    elif [[ "${line,,}" == *"dimm b"* && "$channelBexhausted" == "false" ]];then
        echo "false" > "$root_path/data/tmp/chDetected.tmp"
        echo "true" > "$root_path/data/tmp/chB.tmp"
        channels=$((channels + 1))
        echo "$channels" > "$dataDir/channels.txt"
    elif [[ "$line" == *"CHA"* && "$channelAexhausted" == "false" ]];then
        echo "false" > "$root_path/data/tmp/chDetected.tmp"
        echo "true" > "$root_path/data/tmp/chA.tmp"
        channels=$((channels + 1))
        echo "$channels" > "$dataDir/channels.txt"
    elif [[ "$line" == *"CHB"* && "$channelBexhausted" == "false" ]];then
        echo "false" > "$root_path/data/tmp/chDetected.tmp"
        echo "true" > "$root_path/data/tmp/chB.tmp"
        channels=$((channels + 1))
        echo "$channels" > "$dataDir/channels.txt"
    else
        echo "Detection Attempt failed once. The found string: $line" >> "$errPath/fetch-info.txt"
    fi
fi
done
channelAexhausted=$(cat "$root_path/data/tmp/chA.tmp")
channelBexhausted=$(cat "$root_path/data/tmp/chB.tmp")
if [[ "$channelAexhausted" == "false" && "$channelBexhausted" == "false" ]];then
  echo "Could not detect channels. Assumed the channels based on the DIMMs" >> "$errPath/fetch-info.txt"
  echo "$dimmNum" > "$dataDir/channels.txt"
fi
echo -e "${BLUE_BG} Fetch ${COLOR_RESET} Number of channels Detected - ${GREEN_TXT}SUCCESS${COLOR_RESET}"
# ------------------------------------- Channel Detection End -------------------------------------

echo "100%" > "$progress"
