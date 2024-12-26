#!/bin/bash

if [ "$(ps -aux | grep "main\.sh" | wc -l)" -ne 2 ]; then
	echo "The script main.sh is already running. Not executing it again."
	exit -1
fi

source conf.cfg

mkdir -p "$root_path/data/tmp"
mkdir -p "$root_path/data/errors"
mkdir -p "$root_path/data/logs"

root_path=$ROOT_PATH
errPath="$root_path/data/errors"
progressStage2="$root_path/data/tmp/stage2.tmp"
progressStage3="$root_path/data/tmp/stage3.tmp"
progressStage4="$root_path/data/tmp/stage4.tmp"
skipped_tools_log_path="$root_path/data/logs/skipped_tools.log"
> "$skipped_tools_log_path"
ARHE_START_TIMESTAMP=$(date +%s)
isFlag="false"
HAMMER_TOOLS_NUM=0

endExperiment() {
	python3 ./components/tokenMaker.py
	bash ./components/genSummary.sh
    if [ "$isFlag" = "false" ];then
        bash ./components/compress.sh
        bash ./components/upload.sh -q
    fi
	echo -e "${BLACK_TXT}${GREEN_BG} EXPERIMENT DONE! ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] The test is now completed! We hope you had a good experience and we are grateful for helping us making the world a safer place for everyone!"
	echo "100%" > "$progressStage4"
	exit 0
}

logDisabledTools() {
	if [ "$ENABLE_REAF_Drama" -eq 0 ]; then
		echo "Drama|disabled" >> "$skipped_tools_log_path"
	fi

	if [ "$ENABLE_REAF_AMDRE" -eq 0 ]; then
		echo "AMDRE|disabled" >> "$skipped_tools_log_path"
	fi

	if [ "$ENABLE_REAF_TRRespass" -eq 0 ]; then
		echo "TRRespassRE|disabled" >> "$skipped_tools_log_path"
	fi

	if [ "$ENABLE_REAF_DRAMDig" -eq 0 ]; then
		echo "DRAMDig|disabled" >> "$skipped_tools_log_path"
	fi

	if [ "$ENABLE_REAF_Dare" -eq 0 ]; then
		echo "Dare|disabled" >> "$skipped_tools_log_path"
	fi

	if [ "$ENABLE_ROWHAMMER_Blacksmith" -eq 0 ]; then
		echo "Blacksmith|disabled" >> "$skipped_tools_log_path"
	fi

	if [ "$ENABLE_ROWHAMMER_TRRespass" -eq 0 ]; then
		echo "TRRespass|disabled" >> "$skipped_tools_log_path"
	fi

	if [ "$ENABLE_ROWHAMMER_FlipFloyd" -eq 0 ]; then
		echo "FlipFloyd|disabled" >> "$skipped_tools_log_path"
	fi

	if [ "$ENABLE_ROWHAMMER_RowPress" -eq 0 ]; then
		echo "RowPress|disabled" >> "$skipped_tools_log_path"
	fi

	if [ "$ENABLE_ROWHAMMER_HammerTool" -eq 0 ]; then
		echo "HammerTool|disabled" >> "$skipped_tools_log_path"
	fi

	if [ "$ENABLE_ROWHAMMER_RowhammerJs" -eq 0 ]; then
		echo "RowhammerJs|disabled" >> "$skipped_tools_log_path"
	fi

	if [ "$ENABLE_ROWHAMMER_RowhammerTest" -eq 0 ]; then
		echo "Rowhammer_test|disabled" >> "$skipped_tools_log_path"
	fi

	# if [ "$ENABLE_ROWHAMMER_Sledgehammer" -eq 0 ]; then
	# 	echo "Sledgehammer|disabled" >> "$skipped_tools_log_path"
	# fi

	# if [ "$ENABLE_ROWHAMMER_Zenhammer" -eq 0 ]; then
	# 	echo "Zenhammer|disabled" >> "$skipped_tools_log_path"
	# fi
}

countRemainingTools() {
	HAMMER_TOOLS_NUM=0
	local arr=(\
			"$ENABLE_ROWHAMMER_Blacksmith" \
			"$ENABLE_ROWHAMMER_TRRespass" \
			"$ENABLE_ROWHAMMER_FlipFloyd" \
			"$ENABLE_ROWHAMMER_RowPress"\
			"$ENABLE_ROWHAMMER_RowhammerTest" \
			"$ENABLE_ROWHAMMER_HammerTool" \
			"$ENABLE_ROWHAMMER_RowhammerJs" \
			# "$ENABLE_ROWHAMMER_Sledgehammer" \
			# "$ENABLE_ROWHAMMER_Zenhammer" \
	)
	local element
	for element in "${arr[@]}"; do
    	if [ "$element" = "1" ];then
			HAMMER_TOOLS_NUM=$((HAMMER_TOOLS_NUM + 1))
		fi
	done
}

adjustMaxPercentage() {
	if [[ "$PERCENTAGE_TOOLS" == "0" ]];then
		echo "0" > "$root_path/data/tmp/set_max_percentage.tmp"
	else
		local diff=$((PERCENTAGE_TOOLS - HAMMER_TOOLS_NUM))
		diff=$((diff + 1))
		local max_percentage=$((diff * PERCENTAGE_MAX_INITIAL))
		if [ $max_percentage -gt 100 ];then
			max_percentage=100
		fi
		echo "$max_percentage" > "$root_path/data/tmp/set_max_percentage.tmp"
	fi
}


adjustScriptRuntimeOnce() {
	local now=$(date +%s)
	local diff=$((now - ARHE_START_TIMESTAMP))
	local script_runtime=$(cat "$root_path/data/script_runtime.txt")
	local new_script_runtime=$((script_runtime - diff))
	echo "$new_script_runtime" > "$root_path/data/tmp/total_available_runtime.tmp"
	ARHE_POST_TIMESTAMP="$now"
}

adjustTotalAvailableRuntime() {
	local now=$(date +%s)
	local diff=$((now - ARHE_POST_TIMESTAMP))
	if [ "$diff" -lt "1" ];then
		diff=0
	fi
	local total_available_runtime=$(cat "$root_path/data/tmp/total_available_runtime.tmp")
	local new_total_available_runtime=$((total_available_runtime - diff))
	echo "$new_total_available_runtime" > "$root_path/data/tmp/total_available_runtime.tmp"
	ARHE_POST_TIMESTAMP="$now"
}

adjustAvailableRuntime() {
	adjustTotalAvailableRuntime
	countRemainingTools
	adjustMaxPercentage
	local total_available_runtime=$(cat "$root_path/data/tmp/total_available_runtime.tmp")
	if [[ "$HAMMER_TOOLS_NUM" == "0" ]];then
		echo "0" > "$root_path/data/tmp/available_runtime.tmp"
	else
		local available_runtime=$((total_available_runtime / HAMMER_TOOLS_NUM))
		echo "$available_runtime" > "$root_path/data/tmp/available_runtime.tmp"
	fi
}

disableToolInConf() {
	tool_name=$2
	reason=$3
	if [[ -n "$tool_name" && -n "$reason" ]];then
		if ! grep -Fxq "$tool_name|$reason" "$skipped_tools_log_path"; then
			echo "$tool_name|$reason" >> "$skipped_tools_log_path"
		fi
	fi
	sed -i 's/^'$1'=1$/'$1'=0/g' conf.cfg
	source conf.cfg
}

runRowhammerTool() {
	local global_var_name=$1
	local tool_name=$2
	local global_var_value
	eval "global_var_value=\$$global_var_name"

	if [ "$global_var_value" == 1 ]; then
		echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Starting $tool_name Experiment ..."
		bash ./components/rowhammer/build$tool_name.sh
		if [ "$?" -ne 0 ]; then
			echo -e "${RED_TXT} Rowhammer ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Failed to build $tool_name, skipping Experiment."
			disableToolInConf "$global_var_name" "$tool_name" "build"
		else
			echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Building $tool_name was successful, starting Experiment..."
			bash ./components/rowhammer/run$tool_name.sh
			if [ "$?" -ne 0 ]; then
				echo -e "${RED_TXT} Rowhammer ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] $tool_name Experiment failed."
				disableToolInConf "$global_var_name" "$tool_name" "failed"
			else
				echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] $tool_name Experiment Ended Successfully!"
				disableToolInConf "$global_var_name" "$tool_name" "ended"
			fi

		fi
	fi
}

disableREAF() {
	local script_runtime=$(cat "$root_path/data/script_runtime.txt")
	if [[ "$script_runtime" -lt "14400" ]];then
		disableToolInConf "ENABLE_REAF_Drama" "DRAMA" "runtime"
		disableToolInConf "ENABLE_REAF_AMDRE" "AMDRE" "runtime"
		disableToolInConf "ENABLE_REAF_DRAMDig" "DRAMDig" "runtime"
		disableToolInConf "ENABLE_REAF_Dare" "Dare" "runtime"
	elif [[ "$script_runtime" -lt "18000" ]];then
		disableToolInConf "ENABLE_REAF_Dare" "Dare" "runtime"
		disableToolInConf "ENABLE_REAF_AMDRE" "AMDRE" "runtime"
		disableToolInConf "ENABLE_REAF_DRAMDig" "DRAMDig" "runtime"
	elif [[ "$script_runtime" -lt "21600" ]];then
		disableToolInConf "ENABLE_REAF_AMDRE" "AMDRE" "runtime"
		disableToolInConf "ENABLE_REAF_DRAMDig" "DRAMDig" "runtime"
	elif [[ "$script_runtime" -lt "25200" ]];then
		disableToolInConf "ENABLE_REAF_DRAMDig" "DRAMDig" "runtime"
	fi
	> "$root_path/data/amdre-functions.txt"
	> "$root_path/data/dare-functions.txt"
	> "$root_path/data/drama-functions.txt"
	> "$root_path/data/trrespass-functions.txt"
	> "$root_path/data/dramdig-functions.txt"
}


while getopts "u" opt; do
  case $opt in
    u)
      isFlag="true"
      ;;
    *)
      echo "Usage: $0 [-u] | Do not use this flag | Only used for internal handling of UI"
      exit 1
      ;;
  esac
done

if [ "$isFlag" = "false" ];then
    echo "$DEFAULT_RUNTIME" > "$root_path/data/script_runtime.txt"
fi

if [[ $EUID -ne 0 ]]; then
   echo -e "${RED_BG} ERROR ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] ARHE must be run as root! Please re-run the script as ${GREEN_BG} sudo ${COLOR_RESET}" >&2
   exit 1
fi

logDisabledTools
disableREAF

bash ./components/intro.sh
bash ./components/build.sh

echo -e "----------------------------------------\n${BLUE_BG} Fetch ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Initiating system information fetching ..."
bash ./components/fetch-info.sh
echo -e "${BLUE_BG} Fetch ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] ${GREEN_TXT}System information fetched successfully!${COLOR_RESET}"

# BANK Retrieve & RE Start
############
echo -e "----------------------------------------\n${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Initiating REAF ..."
bash ./components/banks.sh

bankScriptStatus=$?
if [ "$bankScriptStatus" = 1 ]; then
	echo -e "${BLACK_TXT}${GREEN_BG} THANK YOU! ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Sadly, We can not continue with our tests. We will let you know once we solve this problem. Thank you for your efforts!"
	echo "[$(date +%Y-%m-%d_%H-%M-%S)] | Number of banks are empty" >> "$errPath/banks.log"
fi

if [ "$bankScriptStatus" = 2 ]; then
	echo -e "${BLACK_TXT}${GREEN_BG} THANK YOU! ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Sadly, We can not continue with our tests. We will let you know once we solve this problem. Thank you for your efforts!"
	echo "[$(date +%Y-%m-%d_%H-%M-%S)] | Number of banks not power of 2" >> "$errPath/banks.log"
fi

if [ "$bankScriptStatus" = 3 ]; then
	echo -e "${BLACK_TXT}${GREEN_BG} THANK YOU! ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Sadly, We can not continue with our tests. We will let you know once we solve this problem. Thank you for your efforts!"
	echo "[$(date +%Y-%m-%d_%H-%M-%S)] | Number of DIMMs not power of 2" >> "$errPath/banks.log"
fi

if [ "$bankScriptStatus" = 4 ]; then
	echo -e "${BLACK_TXT}${GREEN_BG} THANK YOU! ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Sadly, We can not continue with our tests. We will let you know once we solve this problem. Thank you for your efforts!"
	echo "[$(date +%Y-%m-%d_%H-%M-%S)] | DIMMs number from dmidecode and decode-dimms are not the same." >> "$errPath/banks.log"
fi

if [ "$bankScriptStatus" = 5 ]; then
	echo -e "${BLACK_TXT}${GREEN_BG} THANK YOU! ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Sadly, We can not continue with our tests. We will let you know once we solve this problem. Thank you for your efforts!"
	echo "[$(date +%Y-%m-%d_%H-%M-%S)] | Number of banks could not be retrieved correctly" >> "$errPath/banks.log"
fi

# HugePage Handling
############
bash "$root_path/components/utils/hugepage.sh"
HG1Status=$?
if [ $HG1Status -ne 0 ]; then
	disableToolInConf "ENABLE_REAF_TRRespass" "TRRespassRE" "hugepage"
	disableToolInConf "ENABLE_REAF_DRAMDig" "DRAMDig" "hugepage"
	disableToolInConf "ENABLE_REAF_Dare" "Dare" "hugepage"
	disableToolInConf "ENABLE_ROWHAMMER_Blacksmith" "Blacksmith" "hugepage"
	disableToolInConf "ENABLE_ROWHAMMER_TRRespass" "TRRespass" "hugepage"
	# disableToolInConf "ENABLE_ROWHAMMER_Zenhammer" "Zenhammer" "hugepage"
	disableToolInConf "ENABLE_ROWHAMMER_RowPress" "RowPress" "hugepage"
# else
# 	bash "$root_path/components/utils/hugepage.sh" -m
# 	HG2Status=$?
# 	if [ $HG2Status -ne 0 ]; then
# 		disableToolInConf "ENABLE_ROWHAMMER_Zenhammer" "Zenhammer" "hugepage"
# 	fi
fi

# DRAMA REAF
############
if [ "$ENABLE_REAF_Drama" == 1 ]; then
	echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Initiating DRAMA ..."
	bash ./components/REAF/drama.sh
	dramaStatus=$?
	echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Initiating python script ..."
	python3 "$root_path/components/utils/handleDramaData.py"
	if [ "$dramaStatus" != "0" ]; then
		echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] ${RED_TXT}Failed to retrieve DRAMA addressing functions for 4 times${COLOR_RESET} | ${GREEN_TXT}Moving on to next stages ...${COLOR_RESET}"
		echo "[$(date +%Y-%m-%d_%H-%M-%S)] | DRAMA addressing functions failed" >> "$errPath/drama.log"
	else
		echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] ${GREEN_TXT}DRAMA addressing functions stored successfully!${COLOR_RESET}"
	fi
fi

# AMDRE REAF
############
if [ "$ENABLE_REAF_AMDRE" == 1 ]; then
	echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Initiating AMDRE ..."
	bash ./components/REAF/amdre.sh
	amdreStatus=$?
	if [ "$amdreStatus" != "0" ]; then
		echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] ${RED_TXT}Failed to retrieve AMDRE addressing functions${COLOR_RESET} | ${GREEN_TXT}Moving on to next stages ...${COLOR_RESET}"
		echo "[$(date +%Y-%m-%d_%H-%M-%S)] | AMDRE addressing functions failed" >> "$errPath/amdre.log"
	else
		echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] ${GREEN_TXT}AMDRE addressing functions stored successfully!${COLOR_RESET}"
	fi
fi

# TRRespass REAF
############
if [ "$ENABLE_REAF_TRRespass" == 1 ]; then
	echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Initiating TRRespass RE. Please Wait for a while ..."
	bash ./components/REAF/trrespass.sh
	trrespassREStatus=$?
	if [ "$trrespassREStatus" != "0" ]; then
		echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] ${RED_TXT}Failed to retrieve TRRespass addressing functions${COLOR_RESET} | ${GREEN_TXT}Moving on to next stages ...${COLOR_RESET}"
		echo "[$(date +%Y-%m-%d_%H-%M-%S)] | TRRespass addressing functions failed" >> "$errPath/trrespassRE.log"
	else
		echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] ${GREEN_TXT}TRRespass addressing functions stored successfully!${COLOR_RESET}"
	fi
fi

# DRAMDig REAF
############
if [ "$ENABLE_REAF_DRAMDig" == 1 ]; then
	echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Initiating DRAMDig ..."
	bash ./components/REAF/dramdig.sh
	dramdigStatus=$?
	if [ "$dramdigStatus" != "0" ]; then
		echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] ${RED_TXT}Failed to retrieve DRAMDig addressing functions${COLOR_RESET} | ${GREEN_TXT}Moving on to next stages ...${COLOR_RESET}"
		echo "[$(date +%Y-%m-%d_%H-%M-%S)] | DRAMDig addressing functions failed" >> "$errPath/dramdig.log"
	else
		echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] ${GREEN_TXT}DRAMDig addressing functions stored successfully!${COLOR_RESET}"
	fi
fi

# Dare REAF
############
if [ "$ENABLE_REAF_Dare" == 1 ]; then
	echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Initiating Dare (Zenhammer) ..."
	bash ./components/REAF/dare.sh
	dareStatus=$?
	if [ "$dareStatus" != "0" ]; then
		echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] ${RED_TXT}Failed to retrieve Dare (Zenhammer) addressing functions${COLOR_RESET} | ${GREEN_TXT}Moving on to next stages ...${COLOR_RESET}"
		echo "[$(date +%Y-%m-%d_%H-%M-%S)] | Dare (Zenhammer) addressing functions failed" >> "$errPath/dare.log"
	else
		echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] ${GREEN_TXT}Dare (Zenhammer) addressing functions stored successfully!${COLOR_RESET}"
	fi
fi
echo "100%" > "$progressStage2"

# Addressing Functions Verification
############
echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Handling current pool of functions ..."
bash ./components/utils/handlePool.sh > /dev/null 2>&1
echo "40%" > "$progressStage3"
bash ./components/REAF/drama-verify.sh
echo "90%" > "$progressStage3"
echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Handling Final Functions to prepare for rowhammering ..."
bash ./components/utils/handleFinalFunctions.sh > /dev/null 2>&1
echo "100%" > "$progressStage3"
echo -e "${BLACK_TXT}${GREEN_BG} REAF ${COLOR_RESET} [$(date +%Y-%m-%d_%H-%M-%S)] Final Functions are ready!"

# RE final check
############
F_NUM=$(cat "$root_path/data/functions-num.txt")
if [ -f "$root_path/data/reaf-pool.txt" ]; then
	function_count=$(cat "$root_path/data/reaf-pool.txt" | tr -cd ',' | wc -c)
	function_count=$((function_count + 1))
	if [ "$function_count" -ge "$F_NUM" ]; then
    	didREsucceed="true"
	else
		didREsucceed="false"
	fi
else
	didREsucceed="false"
fi
if [ "$didREsucceed" = "false" ]; then
	disableToolInConf "ENABLE_ROWHAMMER_Blacksmith" "Blacksmith" "addressing function"
	disableToolInConf "ENABLE_ROWHAMMER_TRRespass" "TRRespass" "addressing function"
	disableToolInConf "ENABLE_ROWHAMMER_Zenhammer" "Zenhammer" "addressing function"
	disableToolInConf "ENABLE_ROWHAMMER_RowPress" "RowPress" "addressing function"
	disableToolInConf "ENABLE_ROWHAMMER_HammerTool" "HammerTool" "addressing function"
	disableToolInConf "ENABLE_ROWHAMMER_RowhammerJs" "RowhammerJs" "addressing function"
	# disableToolInConf "ENABLE_ROWHAMMER_Sledgehammer" "Sledgehammer" "addressing function"
fi

# Prepare Percentage Calculator For Rowhammer Tools
############
countRemainingTools
PERCENTAGE_TOOLS="$HAMMER_TOOLS_NUM"
if [[ "$PERCENTAGE_TOOLS" == "0" ]];then
	echo "0" > "$root_path/data/tmp/set_max_percentage.tmp"
else
	PERCENTAGE_MAX_INITIAL=$((100 / PERCENTAGE_TOOLS))
	echo "$PERCENTAGE_MAX_INITIAL" > "$root_path/data/tmp/set_max_percentage.tmp"
fi
HAMMER_TOOLS_NUM=0

# Update the script runtime
############
adjustScriptRuntimeOnce

# Blacksmith RH Tool
############
adjustAvailableRuntime
runRowhammerTool "ENABLE_ROWHAMMER_Blacksmith" "Blacksmith"

# TRRespass RH Tool
############
adjustAvailableRuntime
runRowhammerTool "ENABLE_ROWHAMMER_TRRespass" "TRRespass"

# HammerTool RH Tool
############ (new format, please use this for the tools you are adding)
adjustAvailableRuntime
runRowhammerTool "ENABLE_ROWHAMMER_HammerTool" "HammerTool"

# FlipFloyd RH Tool
############
adjustAvailableRuntime
runRowhammerTool "ENABLE_ROWHAMMER_FlipFloyd" "FlipFloyd"

# RowPress RH Tool
############
adjustAvailableRuntime
runRowhammerTool "ENABLE_ROWHAMMER_RowPress" "RowPress"

# Rowhammer.JS RH Tool
############
adjustAvailableRuntime
runRowhammerTool "ENABLE_ROWHAMMER_RowhammerJs" "RowhammerJs"

# Rowhammer-test RH Tool
############
adjustAvailableRuntime
runRowhammerTool "ENABLE_ROWHAMMER_RowhammerTest" "Rowhammer_test"

endExperiment
