#!/bin/bash

source conf.cfg

root_path=$ROOT_PATH
dir="$root_path/data"
logFile="$dir/logs/main.log"

getTimestamp() {
	timestamp="$(echo "${1}" | sed 's/.*\[\([0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}_[0-9]\{2\}-[0-9]\{2\}-[0-9]\{2\}\)\].*/\1/g')"
	if [ "$timestamp" == "" ]; then
		echo "0000-00-00_00-00-00"
	else
		echo "$timestamp"
	fi
}

getDifference() {
	if [ "${1}" == "0000-00-00_00-00-00" ] || [ "${2}" == "0000-00-00_00-00-00" ]; then
		echo "0"
	fi

	#year1="$(echo "${1}" | sed 's/.*\[\([0-9]\{4\}\)-[0-9]\{2\}-[0-9]\{2\}_[0-9]\{2\}-[0-9]\{2\}-[0-9]\{2\}\].*/\1/g')"
	#month1="$(echo "${1}" | sed 's/.*\[[0-9]\{4\}-\([0-9]\{2\}\)-[0-9]\{2\}_[0-9]\{2\}-[0-9]\{2\}-[0-9]\{2\}\].*/\1/g')"
	day1="$(echo "${1}" | sed 's/[0-9]\{4\}-[0-9]\{2\}-\([0-9]\{2\}\)_[0-9]\{2\}-[0-9]\{2\}-[0-9]\{2\}/\1/g')"
	hour1="$(echo "${1}" | sed 's/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}_\([0-9]\{2\}\)-[0-9]\{2\}-[0-9]\{2\}/\1/g')"
	minute1="$(echo "${1}" | sed 's/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}_[0-9]\{2\}-\([0-9]\{2\}\)-[0-9]\{2\}/\1/g')"
	second1="$(echo "${1}" | sed 's/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}_[0-9]\{2\}-[0-9]\{2\}-\([0-9]\{2\}\)/\1/g')"

	#year2="$(echo "${2}" | sed 's/.*\[\([0-9]\{4\}\)-[0-9]\{2\}-[0-9]\{2\}_[0-9]\{2\}-[0-9]\{2\}-[0-9]\{2\}\].*/\1/g')"
	#month2="$(echo "${2}" | sed 's/.*\[[0-9]\{4\}-\([0-9]\{2\}\)-[0-9]\{2\}_[0-9]\{2\}-[0-9]\{2\}-[0-9]\{2\}\].*/\1/g')"
	day2="$(echo "${2}" | sed 's/[0-9]\{4\}-[0-9]\{2\}-\([0-9]\{2\}\)_[0-9]\{2\}-[0-9]\{2\}-[0-9]\{2\}/\1/g')"
	hour2="$(echo "${2}" | sed 's/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}_\([0-9]\{2\}\)-[0-9]\{2\}-[0-9]\{2\}/\1/g')"
	minute2="$(echo "${2}" | sed 's/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}_[0-9]\{2\}-\([0-9]\{2\}\)-[0-9]\{2\}/\1/g')"
	second2="$(echo "${2}" | sed 's/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}_[0-9]\{2\}-[0-9]\{2\}-\([0-9]\{2\}\)/\1/g')"

	echo "($day2 - $day1) * (3600 * 24) + ($hour2 - $hour1) * 3600 + ($minute2 - $minute1) * 60 + ($second2 - $second1)" | bc
}

getTime() {
	hours="$(echo "${1} / 3600" | bc)"
	minutes="$(echo "(${1} % 3600) / 60" | bc)"
	seconds="$(echo "(${1} % 60)" | bc)"
	printf "%02d:%02d:%02d" $hours $minutes $seconds
}

echo """# Dataset Summary
This file shows basic information of the dataset. It can be used to get a brief
overview of the results without the necessity to manually parse all information.
However, more details can be obtained by manually looking at the log files.
""" > $dir/summary.md

cpuModel="$(cat $dir/sys-info/lscpu.txt | grep "^Model name:" | sed 's/^[^:]\+: \+\([^ ].*$\)/\1/g')"
cpuArch=""$(cat $dir/sys-info/lscpu.txt | grep "^Architecture:" | sed 's/^[^:]\+: \+\([^ ].*$\)/\1/g')
cpuPhysicalCores="$(cat $dir/sys-info/lscpu.txt | grep "^Core(s) per socket:" | sed 's/^[^:]\+: \+\([^ ].*$\)/\1/g')"
cpuLogicalCores="$(cat $dir/sys-info/lscpu.txt | grep "^CPU(s):" | sed 's/^[^:]\+: \+\([^ ].*$\)/\1/g')"
cpuL1DataCache="$(cat $dir/sys-info/lscpu.txt | grep "^L1d cache:" | sed 's/^[^:]\+: \+\([^ ].*$\)/\1/g')"
cpuL1InstructionCache="$(cat $dir/sys-info/lscpu.txt | grep "^L1i cache:" | sed 's/^[^:]\+: \+\([^ ].*$\)/\1/g')"
cpuL2Cache="$(cat $dir/sys-info/lscpu.txt | grep "^L2 cache:" | sed 's/^[^:]\+: \+\([^ ].*$\)/\1/g')"
cpuL3Cache="$(cat $dir/sys-info/lscpu.txt | grep "^L3 cache:" | sed 's/^[^:]\+: \+\([^ ].*$\)/\1/g')"
echo """## System Information - CPU
* CPU Model: \`$cpuModel\`
* CPU Architecture: \`$cpuArch\`
* Number of physical cores: \`$cpuPhysicalCores\`
* Number of logical cores: \`$cpuLogicalCores\`
* L1 data cache: \`$cpuL1DataCache\`
* L1 instruction cache: \`$cpuL1InstructionCache\`
* L2 cache: \`$cpuL2Cache\`
* L3 cache: \`$cpuL3Cache\`
""" >> $dir/summary.md

nDIMMs="$(cat $dir/sys-info/dmidecode_memory.txt | grep -n "[^ ]Size" | grep -v "No Module Installed" | wc -l)"
for ((dimmIdx=0; dimmIdx < $nDIMMs; dimmIdx++)); do
	startLineNumber="$(cat $dir/sys-info/dmidecode_memory.txt | grep -n "[^ ]Size" | grep -v "No Module Installed" | head -n $(($dimmIdx + 1)) | tail -n 1 | cut -d ":" -f 1)"
	endLineNumbers="$(cat $dir/sys-info/dmidecode_memory.txt | grep -n "^Handle" | cut -d ":" -f 1)"
	endLineNumber=$(($(cat $dir/sys-info/dmidecode_memory.txt | wc -l) + 2))
	for number in $endLineNumbers; do
		if [ "$number" -gt "$startLineNumber" ]; then
			endLineNumber="$number"
			break;
		fi
	done

	content="$(cat $dir/sys-info/dmidecode_memory.txt | head -n +$(($endLineNumber-2)) | tail -n +$(($startLineNumber-4)))"
	dimmType="$(echo "$content" | grep "[^ ]Type: " | cut -d ":" -f 2 | cut -d " " -f 2)"
	dimmSize="$(echo "$content" | grep "[^ ]Size: " | cut -d ":" -f 2 | cut -d " " -f 2)"
	dimmRanks="$(echo "$content" | grep "[^ ]Rank: " | cut -d ":" -f 2 | cut -d " " -f 2)"
	dimmFormFactor="$(echo "$content" | grep "[^ ]Form Factor: " | cut -d ":" -f 2 | cut -d " " -f 2)"
	dimmLocator="$(echo "$content" | grep "[^ ]Locator: " | cut -d ":" -f 2 | cut -d " " -f 2)"
	dimmSpeed="$(echo "$content" | grep "[^ ]Configured Memory Speed: " | cut -d ":" -f 2 | cut -d " " -f 2)"
	dimmVoltage="$(echo "$content" | grep "[^ ]Configured Voltage: " | cut -d ":" -f 2 | cut -d " " -f 2)"
	dimmManufacturer="$(echo "$content" | grep "[^ ]Manufacturer: " | cut -d ":" -f 2 | cut -d " " -f 2)"
	dimmModel="$(echo "$content" | grep "[^ ]Part Number: " | cut -d ":" -f 2 | cut -d " " -f 2)"
	dimmSerialNumber="$(echo "$content" | grep "[^ ]Serial Number: " | cut -d ":" -f 2 | cut -d " " -f 2)"
	echo """## System Information - DIMM $dimmIdx
* DIMM Type: \`$dimmType\`
* DIMM Size: \`$dimmSize\`
* DIMM Ranks: \`$dimmRanks\`
* DIMM Form factor: \`$dimmFormFactor\`
* DIMM Locator: \`$dimmLocator\`
* DIMM Speed: \`$dimmSpeed\`
* DIMM Voltage: \`$dimmVoltage\`
* DIMM Manufacturer: \`$dimmManufacturer\`
* DIMM Model: \`$dimmModel\`
* DIMM Serial Number: \`$dimmSerialNumber\`
	""" >> $dir/summary.md
done

afnFinal="(none)"
if [ "$(cat $dir/final-functions.txt | wc -c)" -gt 1 ]; then
	afnFinal="$(cat $dir/final-functions.txt)"
fi

afnAMDRE="(none)"
if [ "$(cat $dir/amdre-functions.txt | wc -c)" -gt 1 ]; then
	afnAMDRE="$(cat $dir/amdre-functions.txt)"
fi

afnDrama="(none)"
if [ "$(cat $dir/drama-functions.txt | wc -c)" -gt 1 ]; then
	afnDrama="$(cat $dir/drama-functions.txt | tail -n +2 | cut -d ":" -f 3 | cut -d "," -f 1 | tr '\n' ',' | sed 's/^ //g;s/,$//g')"
fi

afnTRRespass="(none)"
if [ "$(cat $dir/trrespass-functions.txt | wc -c)" -gt 1 ]; then
	afnTRRespass="$(cat $dir/trrespass-functions.txt)"
fi

afnDare="(none)"
if [ "$(cat $dir/dare-functions.txt | wc -c)" -gt 1 ]; then
	afnDare="$(cat $dir/dare-functions.txt)"
fi

afnDRAMDig="(none)"
if [ "$(cat $dir/dramdig-functions.txt | wc -c)" -gt 1 ]; then
	afnDRAMDig="$(cat $dir/dramdig-functions.txt)"
fi

startTime="$(getTimestamp "$(cat $logFile | grep "Initiating AMDRE ...")")"
endTime="$(getTimestamp "$(cat $logFile | grep "AMDRE addressing functions")")"
AMDRERuntime="$(getTime $(getDifference ${startTime} ${endTime}))"

startTime="$(getTimestamp "$(cat $logFile | grep "Initiating DRAMA ...")")"
endTime="$(getTimestamp "$(cat $logFile | grep "DRAMA addressing functions")")"
DramaRuntime="$(getTime $(getDifference ${startTime} ${endTime}))"

startTime="$(getTimestamp "$(cat $logFile | grep "Initiating TRRespass RE. Please Wait for a while ...")")"
endTime="$(getTimestamp "$(cat $logFile | grep "TRRespass addressing functions")")"
TRRespassRuntime="$(getTime $(getDifference ${startTime} ${endTime}))"

startTime="$(getTimestamp "$(cat $logFile | grep "Initiating Dare (Zenhammer) ...")")"
endTime="$(getTimestamp "$(cat $logFile | grep "Dare (Zenhammer) addressing functions")")"
DareRuntime="$(getTime $(getDifference ${startTime} ${endTime}))"

startTime="$(getTimestamp "$(cat $logFile | grep "Initiating DRAMDig ...")")"
endTime="$(getTimestamp "$(cat $logFile | grep "DRAMDig addressing functions")")"
DRAMDigRuntime="$(getTime $(getDifference ${startTime} ${endTime}))"
echo """## System Information: DRAM Addressing
* Final Bank Addressing Functions: \`$afnFinal\`
* AMDRE Bank Addressing functions: \`$afnAMDRE\` (took $AMDRERuntime)
* Drama Bank Addressing functions: \`$afnDrama\` (took $DramaRuntime)
* TRRespass Bank Addressing functions: \`$afnTRRespass\` (took $TRRespassRuntime)
* Dare Bank Addressing functions: \`$afnDare\` (took $DareRuntime)
* DRAMDig Bank Addressing functions: \`$afnDRAMDig\` (took $DRAMDigRuntime)
""" >> $dir/summary.md

startTime="$(getTimestamp "$(cat $logFile | grep "Starting Blacksmith Experiment ...")")"
endTime="$(getTimestamp "$(cat $logFile | grep "Blacksmith Experiment Ended Successfully!")")"
blacksmithRuntime="$(getTime $(getDifference ${startTime} ${endTime}))"
blacksmithBitFlips="(Skipped)"
if [ -f $dir/bit-flip-reports-blacksmith.txt ]; then
	blacksmithBitFlips="$(cat $dir/bit-flip-reports-blacksmith.txt | grep "Total Bit Flips" | cut -d ":" -f 2 | cut -d " " -f 2)"
fi

startTime="$(getTimestamp "$(cat $logFile | grep "Starting TRRespass Experiment ...")")"
endTime="$(getTimestamp "$(cat $logFile | grep "TRRespass Experiment Ended Successfully!")")"
trrespassRuntime="$(getTime $(getDifference ${startTime} ${endTime}))"
trrespassBitFlips="(Skipped)"
if [ -f $dir/bit-flip-reports-trrespass.txt ]; then
	trrespassBitFlips="$(cat $dir/bit-flip-reports-trrespass.txt | grep "Total Bit Flips" | cut -d ":" -f 2 | cut -d " " -f 2)"
fi

startTime="$(getTimestamp "$(cat $logFile | grep "Starting HammerTool Experiment ...")")"
endTime="$(getTimestamp "$(cat $logFile | grep "HammerTool Experiment Ended Successfully!")")"
hammertoolRuntime="$(getTime $(getDifference ${startTime} ${endTime}))"
hammertoolBitFlips="(Skipped)"
if [ -f $dir/bit-flip-reports-hammertool.txt ]; then
	hammertoolBitFlips="$(cat $dir/bit-flip-reports-hammertool.txt | grep "Total Bit Flips" | cut -d ":" -f 2 | cut -d " " -f 2)"
fi

startTime="$(getTimestamp "$(cat $logFile | grep "Starting FlipFloyd Experiment ...")")"
endTime="$(getTimestamp "$(cat $logFile | grep "FlipFloyd Experiment Ended Successfully!")")"
flipfloydRuntime="$(getTime $(getDifference ${startTime} ${endTime}))"
flipfloydBitFlips="(Skipped)"
if [ -f $dir/bit-flip-reports-flipfloyd.txt ]; then
	flipfloydBitFlips="$(cat $dir/bit-flip-reports-flipfloyd.txt | grep "Total Bit Flips" | cut -d ":" -f 2 | cut -d " " -f 2)"
fi

startTime="$(getTimestamp "$(cat $logFile | grep "Starting RowPress Experiment ...")")"
endTime="$(getTimestamp "$(cat $logFile | grep "RowPress Experiment Ended Successfully!")")"
rowpressRuntime="$(getTime $(getDifference ${startTime} ${endTime}))"
rowpressBitFlips="(Skipped)"
if [ -f $dir/bit-flip-reports-rowpress.txt ]; then
	rowpressBitFlips="$(cat $dir/bit-flip-reports-rowpress.txt | grep "Total Bit Flips" | cut -d ":" -f 2 | cut -d " " -f 2)"
fi

startTime="$(getTimestamp "$(cat $logFile | grep "Starting RowhammerJs Experiment ...")")"
endTime="$(getTimestamp "$(cat $logFile | grep "RowhammerJs Experiment Ended Successfully!")")"
rowhammerjsRuntime="$(getTime $(getDifference ${startTime} ${endTime}))"
rowhammerjsBitFlips="(Skipped)"
if [ -f $dir/bit-flip-reports-rowhammerjs.txt ]; then
	rowhammerjsBitFlips="$(cat $dir/bit-flip-reports-rowhammerjs.txt | grep "Total Bit Flips" | cut -d ":" -f 2 | cut -d " " -f 2)"
fi

startTime="$(getTimestamp "$(cat $logFile | grep "Starting Rowhammer_test Experiment ...")")"
endTime="$(getTimestamp "$(cat $logFile | grep "Rowhammer_test Experiment Ended Successfully!")")"
rowhammertestRuntime="$(getTime $(getDifference ${startTime} ${endTime}))"
rowhammertestBitFlips="(Skipped)"
if [ -f $dir/bit-flip-reports-rowhammertest.txt ]; then
	rowhammertestBitFlips="$(cat $dir/bit-flip-reports-rowhammertest.txt | grep "Total Bit Flips" | cut -d ":" -f 2 | cut -d " " -f 2)"
fi

echo """## Results
* BlackSmith: (took $blacksmithRuntime): \`$blacksmithBitFlips\`
* TRRespass: (took $trrespassRuntime): \`$trrespassBitFlips\`
* HammerTool: (took $hammertoolRuntime): \`$hammertoolBitFlips\`
* FlipFloyd: (took $flipfloydRuntime): \`$flipfloydBitFlips\`
* RowPress: (took $rowpressRuntime): \`$rowpressBitFlips\`
* RowhammerJs: (took $rowhammerjsRuntime): \`$rowhammerjsBitFlips\`
* Rowhammer Test: (took $rowhammertestRuntime): \`$rowhammertestBitFlips\`
""" >> $dir/summary.md

startTime="$(getTimestamp "$(cat $logFile | grep "Initiating system information fetching ...")")"
endTime="$(getTimestamp "[$(date +%Y-%m-%d_%H-%M-%S)]")"
totalRuntime="$(getTime $(getDifference ${startTime} ${endTime}))"

echo -n """## Total Runtime:
$totalRuntime
""" >> $dir/summary.md
