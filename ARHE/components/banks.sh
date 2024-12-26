#!/bin/bash

source conf.cfg

is_power_of_2() {
    local num=$1
    if [ -z $num ]; then
        echo false
    else
        if [ "$num" -gt 0 ] && [ $((num & (num - 1))) -eq 0 ]; then
            echo true
        else
            echo false
        fi
    fi
}

log_calc() {
  local value=$1
  local base=2
  echo | awk "{ print log($value) / log($base) }"
}

root_path=$ROOT_PATH
data_holder1="$root_path/data/tmp/banksTmp1.tmp"
data_holder2="$root_path/data/tmp/banksTmp2.tmp"
log_file="$root_path/data/logs/banks.log"
thresholdLogPath="$root_path/data/logs/thresholdBanks.log"
memClass=$(cat "$root_path/data/sys-info/class.txt")
dimmNum=$(cat "$root_path/data/sys-info/dimm-numbers.txt")
dd=$(cat "$root_path/data/sys-info/dmidecode_memory.txt")
ddm=$(cat "$root_path/data/sys-info/decode-dimms.txt")
isAmd=$(cat "$root_path/data/isAmd.txt")
isDimmNumCorrect=$(cat "$root_path/data/isDimmNumCorrect.txt")
progress="$root_path/data/tmp/stage2.tmp"

> "$data_holder1"
> "$data_holder2"
echo "-------------$(date +%Y-%m-%d_%H-%M-%S)-------------" > "$log_file"

# AMD Arch | Start
if [ "$isAmd" = "true" ];then
  amdrePathIdentifier="AMDRE_New"
else
  amdrePathIdentifier="AMDRE"
fi
# AMD Arch | End

# Memory Class Defining | Start
if [[ "$memClass" == "DDR3" ]];then
    amdrePath="$root_path/Tools/$amdrePathIdentifier/bin/amdre --memory-type=ddr3"
else
    amdrePath="$root_path/Tools/$amdrePathIdentifier/bin/amdre"
fi
# Memory Class Defining | End

# is DIMM number to the power of 2 | Start
if [ "$dimmNum" -gt "2" ];then
    dimmIsPow2=$(is_power_of_2 "$dimmNum")
    if [ "$dimmIsPow2" = false ]; then
        echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Number of DIMMs is not to the power of 2. ${RED_TXT}Shutting down the script!${COLOR_RESET}"
        echo "$banks" > "$root_path/data/banks.txt"
        exit 3
    fi
fi
# is DIMM number to the power of 2 | End

# Retrieving Number of Banks -------------------------------------------------------------------------------------

echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Measuring number of banks ... [Est. ${TIMEOUT_DURATION_BSC}s]"

# Retrieving Rank & Bank data from sys-info
################################################
dmc=1
totalBanks=0
totalRanks=0
while [ "$dmc" -le "$dimmNum" ]
do
    ranks=$(echo "$ddm" | grep "Ranks" | sed -n "${dmc}p" | awk '{print $2}')
    if [ -z "$ranks" ];then
        ranks=$(echo "$dd" | grep "Rank" | sed -n "${dmc}p" | awk '{print $2}')
    fi
    dBanks=$(echo "$ddm"| grep "Banks x Rows x Columns x Bits" | sed -n "${dmc}p" | awk '{print $8}')
    if [ -z "$ranks" ];then
        ranks=1
    fi
    echo "$ranks" > "$root_path/data/logs/blacksmithRanks-$dmc.log"
    if [ -n "$dBanks" ];then
        echo "$dBanks" > "$root_path/data/logs/blacksmithBanks-$dmc.log"
        if [ "$ranks" -gt 1 ];then
            dBanks=$((dBanks * ranks))
        fi
        totalBanks=$((totalBanks + dBanks))
    fi
    totalRanks=$((totalRanks + ranks))
    echo "$totalRanks" > "$root_path/data/ranks.txt"
    dmc=$((dmc + 1))
done
################################################
if [ -z $totalBanks ];then
    totalBanks=0
fi

if [[ "$totalBanks" -gt 0 && "$dimmNum" -gt 1 && "$isDimmNumCorrect" == "true" ]];then
    banks="$totalBanks"
    bankIsEmpty="false"
    fCond="true"
    echo "Bank is retrieved by sys-info" >> "$log_file"
else
    echo "Bank is retrieved by AMDRE" >> "$log_file"
    fCond="false"
    echo "-------------$(date +%Y-%m-%d_%H-%M-%S) | First AMDRE Try -------------" >> "$log_file"
    timeout "$TIMEOUT_DURATION_BSC" stdbuf -oL $amdrePath 2>/dev/null | while IFS= read -r line; do
        echo "$line" | tee "$data_holder1" | sed -r 's/\x1B\[[0-9;]*[JKmsu]//g' > "$data_holder2"
        echo "$line" >> "$log_file"
        echo "$line"
        if [[ "$line" == *"Assuming"* ]]; then
            killall amdre > /dev/null 2>&1
            break
        fi
    done

    banks=$(grep -oP '(?<=Assuming )\d+(?= banks)' "$data_holder2")

    if [ -z "$banks" ]; then
        bankIsEmpty="true"
        banks=0
    else
        bankIsEmpty="false"
    fi

    counter=0

    # Sometimes more retries are needed to retrieve correct number of banks if the bank is less then 8
    ########################
    secondary_retries=$((BANK_RETRIES + 20))
    ########################

    while [[ ( "$counter" -lt "$BANK_RETRIES" && "$bankIsEmpty" == "true" ) || ( "$counter" -lt "$secondary_retries" && "$banks" -lt 8 ) ]]
    do
        counter=$((counter + 1))
        echo "----------------------------------------------------" >> "$log_file"
        echo "-------------$(date +%Y-%m-%d_%H-%M-%S) | Next AMDRE Try: $counter -------------" >> "$log_file"
        echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Measuring number of banks failed. Trying again ... [Est. ${TIMEOUT_DURATION_BSC}s]"
        > "$data_holder1"
        > "$data_holder2"
        timeout "$TIMEOUT_DURATION_BSC" stdbuf -oL $amdrePath 2>/dev/null | while IFS= read -r line; do
            echo "$line" | tee "$data_holder1" | sed -r 's/\x1B\[[0-9;]*[JKmsu]//g' > "$data_holder2"
            echo "$line" >> "$log_file"
            echo "$line"
            if [[ "$line" == *"theshold"* ]]; then
                echo "$line" | sed -r 's/\x1B\[[0-9;]*[JKmsu]//g' > "$thresholdLogPath"
            fi
            if [[ "$line" == *"Assuming"* ]]; then
                killall amdre > /dev/null 2>&1
                break
            fi
        done
        banks=$(grep -oP '(?<=Assuming )\d+(?= banks)' "$data_holder2")
        if [ -n "$banks" ]; then
            bankIsEmpty="false"
        else
            banks=0
        fi
    done
    bb1=$((banks / totalRanks))
    echo "$bb1" > "$root_path/data/logs/blacksmithBanks-1.log"
fi

# If AMDRE fails, get the total banks from sys-info anyways
########################
if [[ "$totalBanks" -gt 0 && ("$banks" -lt 8 || -z "$banks") ]];then
    banks="$totalBanks"
    bankIsEmpty="false"
fi
if [ "$fCond" = "false" ];then
    bb1=$((banks / totalRanks))
    echo "$bb1" > "$root_path/data/logs/blacksmithBanks-1.log"
fi
########################

if [[ "$banks" -gt 7 && "$isDimmNumCorrect" == "true" ]];then
    bankIsPow2=$(is_power_of_2 "$banks")
else
    if [ "$isDimmNumCorrect" = "false" ];then
        echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} DIMMs number is not correct! ${RED_TXT}Shutting down the script!${COLOR_RESET}"
        echo "$banks" > "$root_path/data/banks.txt"
        exit 4
    else
        echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Failed to retrieve number of banks! ${RED_TXT}Shutting down the script!${COLOR_RESET}"
        echo "$banks" > "$root_path/data/banks.txt"
        exit 5
    fi
fi

# End of retrieving Number of Banks -------------------------------------------------------------------------------------

echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Measuring threshold ... (Est. 60s)"

echo "15%" > "$progress"

# Retrieving threshold -------------------------------------------------------------------------------------
timeout "$TIMEOUT_DURATION_BSC" stdbuf -oL $amdrePath 2>/dev/null | while IFS= read -r line; do
    echo "$line"
    if [[ "$line" == *"theshold"* ]]; then
        echo "$line" | sed -r 's/\x1B\[[0-9;]*[JKmsu]//g' > "$thresholdLogPath"
        killall amdre > /dev/null 2>&1
        break
    fi
done

echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Measuring threshold again ... (Est. 60s)"

timeout 60 stdbuf -oL $amdrePath 2>/dev/null | while IFS= read -r line; do
    echo "$line"
    if [[ "$line" == *"theshold"* ]]; then
        echo "$line" | sed -r 's/\x1B\[[0-9;]*[JKmsu]//g' > "$thresholdLogPath"
        killall amdre > /dev/null 2>&1
        break
    fi
done

threshold=$(grep -oP '(?<=theshold: )\d+' "$thresholdLogPath")

if [ -z "$threshold" ]; then
    threshold=10000
fi

threshold_counter=0

while [ "$(echo "$threshold > 5000" | bc)" -eq 1 ] && [ "$threshold_counter" -lt "$THRESHOLD_RETRIES" ]
do
    threshold_counter=$((threshold_counter + 1))
    echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Measuring threshold again ... ${RED_BG} TRY $threshold_counter ${COLOR_RESET}"
    timeout 60 stdbuf -oL "$root_path/Tools/AMDRE/bin/amdre" 2>/dev/null | while IFS= read -r line; do
        echo "$line"
        if [[ "$line" == *"theshold"* ]]; then
            echo "$line" | sed -r 's/\x1B\[[0-9;]*[JKmsu]//g' > "$thresholdLogPath"
            killall amdre > /dev/null 2>&1
            break
        fi
    done
    threshold=$(grep -oP '(?<=theshold: )\d+' "$thresholdLogPath")
    if [ -z "$threshold" ]; then
        threshold=10000
    fi
done
echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Threshold measured ${GREEN_TXT}successfully!${COLOR_RESET}"
# End of Retrieving threshold -------------------------------------------------------------------------------------

if [ "$(echo "$threshold > 5000" | bc)" -eq 1 ];then
    threshold="400" # A default threshold in case AMDRE Fails
fi

if [ "$bankIsEmpty" = "true" ]; then
    echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Failed to measure number of banks. ${RED_TXT}Shutting down the script!${COLOR_RESET}"
    exit 1
elif [ "$bankIsPow2" = false ]; then
    echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Number of banks measured ${GREEN_TXT}successfully!${COLOR_RESET}"
    echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Number of banks is not to the power of 2. ${RED_TXT}Shutting down the script!${COLOR_RESET}"
    echo "$banks" > "$root_path/data/banks.txt"
    exit 2
else
    echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} Number of banks measured ${GREEN_TXT}successfully!${COLOR_RESET}"
    echo "$banks" > "$root_path/data/banks.txt"
    echo "$threshold" > "$root_path/data/threshold.txt"
    echo $(log_calc "$banks") > "$root_path/data/functions-num.txt"
fi
echo "30%" > "$progress"
