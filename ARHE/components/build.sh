#!/bin/bash

source conf.cfg
root_path=$ROOT_PATH

build_measureRefreshRate() {
  echo -e "\nBuilding measureRefreshRate ..."
  root_path="$1"
  cd "$root_path/Tools/measureRefreshRate/"
  logfile="$root_path/data/logs/build.log"
  echo "Start building measureRefreshRate | $(date +%Y-%m-%d_%H-%M-%S)" >> "$logfile"
  make >> "$logfile"  2>&1
  echo "Done building measureRefreshRate | $(date +%Y-%m-%d_%H-%M-%S)" >> "$logfile"
  echo -e "${GREEN_TXT}Done Building measureRefreshRate!${COLOR_RESET}"
}

build_Drama() {
  echo "Building Drama ..."
  root_path="$1"
  cd "$root_path/Tools/Drama/"
  logfile="$root_path/data/logs/build.log"
  echo "Start building Drama | $(date +%Y-%m-%d_%H-%M-%S)" >> "$logfile"
  make >> "$logfile"  2>&1
  echo "Done building Drama | $(date +%Y-%m-%d_%H-%M-%S)" >> "$logfile"
  echo -e "${GREEN_TXT}Done Building Drama!${COLOR_RESET}"
  chmod +x "$root_path/Tools/Drama/measure"
}

build_Dramdig() {
  echo "Building Dramdig ..."
  root_path="$1"
  cd "$root_path/Tools/DRAMDig/"
  logfile="$root_path/data/logs/build.log"
  echo "Start building Dramdig | $(date +%Y-%m-%d_%H-%M-%S)" >> "$logfile"
  make >> "$logfile"  2>&1
  echo "Done building Dramdig | $(date +%Y-%m-%d_%H-%M-%S)" >> "$logfile"
  echo "Starting collection of system information | $(date +%Y-%m-%d_%H-%M-%S)" >> $logfile
  python3 detect_setups.py >> "$logfile" 2>&1
  echo "Done collection of system information | $(date +%Y-%m-%d_%H-%M-%S)" >> $logfile
  echo -e "${GREEN_TXT}Done Building Dramdig!${COLOR_RESET}"
  chmod +x "$root_path/Tools/DRAMDig/dramdig"
}

build_Dare() {
  echo "Building Dare (Zenhammer)..."
  root_path="$1"
  cd "$root_path/Tools/dare/"
  logfile="$root_path/data/logs/build.log"
  echo "Start building Dare (Zenhammer) | $(date +%Y-%m-%d_%H-%M-%S)" >> "$logfile"
  cmake -B build >> "$logfile" 2>&1
  make -C build >> "$logfile" 2>&1
  echo "Done building Dare (Zenhammer) | $(date +%Y-%m-%d_%H-%M-%S)" >> "$logfile"
  echo -e "${GREEN_TXT}Done Building Dare (Zenhammer)!${COLOR_RESET}"
  chmod +x "$root_path/Tools/dare/build/dare"
}

build_AMDRE() {
  echo "Building AMDRE ..."
  root_path="$1"
  cd "$root_path/Tools/AMDRE/"
  logfile="$root_path/data/logs/build.log"
  echo "Start building AMDRE | $(date +%Y-%m-%d_%H-%M-%S)" >> "$logfile"
  make >> "$logfile"  2>&1
  cd "$root_path/Tools/AMDRE_New/"
  make >> "$logfile"  2>&1
  echo "Done building AMDRE | $(date +%Y-%m-%d_%H-%M-%S)" >> "$logfile"
  echo -e "${GREEN_TXT}Done Building AMDRE!${COLOR_RESET}"
}

build_TRRespass() {
  echo "Building TRRespassRE ..."
  root_path="$1"
  cd "$root_path/Tools/TRRespass/"
  logfile="$root_path/data/logs/build.log"
  echo "Start building TRRespassRE | $(date +%Y-%m-%d_%H-%M-%S)" >> "$logfile"
  make >> "$logfile" 2>&1
  echo "Done building TRRespassRE | $(date +%Y-%m-%d_%H-%M-%S)" >> "$logfile"
  echo -e "${GREEN_TXT}Done Building TRRespassRE!${COLOR_RESET}"
}

build_DramaVerify() {
  echo "Building Drama-Verify ..."
  root_path="$1"
  cd "$root_path/Tools/Drama-Verify/"
  logfile="$root_path/data/logs/build.log"
  echo "Start building Drama-Verify | $(date +%Y-%m-%d_%H-%M-%S)" >> "$logfile"
  make >> "$logfile"  2>&1
  echo "Done building Drama-Verify | $(date +%Y-%m-%d_%H-%M-%S)" >> "$logfile"
  echo -e "${GREEN_TXT}Done Building Drama-Verify!${COLOR_RESET}"
}

root_path=$ROOT_PATH

build_measureRefreshRate "$root_path"
build_Drama "$root_path"
build_Dramdig "$root_path"
build_Dare "$root_path"
build_AMDRE "$root_path"
build_TRRespass "$root_path"
build_DramaVerify "$root_path"
