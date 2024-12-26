#!/bin/bash

if [ "$(ps -aux | grep "run\.sh" | grep -v "/bin/sh" | wc -l)" -ne 2 ]; then
	echo "The script run.sh is already running. Not executing it again."
	exit -1
fi

source conf.cfg

mkdir -p data/logs
mkdir -p data/errors
mkdir -p data/tmp

root_path="$ROOT_PATH"
skipped_tools_log_path="$root_path/data/logs/skipped_tools.log"

tput civis

handleDialog() {
	local rc=5
	while [ "$rc" -eq 5 ]; do
		/bin/bash -c "DIALOG_TIMEOUT=5 dialog --timeout ${DIALOG_REDRAW_INTERVAL} ${1}"
		rc=$?
	done 2>/dev/null

	echo $rc > $root_path/data/tmp/dialog_state.tmp
}

check_internet_connection() {
    isLoop=$1
    if [ "$isLoop" = "true" ];then
        while true; do
            if ping -c 1 -W 2 8.8.8.8 > /dev/null 2>&1; then
                echo "Connected" > "$root_path/data/tmp/connection_status.tmp"
            else
                if curl -s -o /dev/null -I -w "%{http_code}" $WEBSITE_URL | grep -q "200"; then
                    echo "Connected" > "$root_path/data/tmp/connection_status.tmp"
                else
                    echo "Not Connected" > "$root_path/data/tmp/connection_status.tmp"
                fi
            fi
            sleep 5
        done
    else
        if ping -c 1 -W 2 8.8.8.8 > /dev/null 2>&1; then
            return 0
        else
            if curl -s -o /dev/null -I -w "%{http_code}" $WEBSITE_URL | grep -q "200"; then
                return 0
            else
                return 1
            fi
        fi
    fi
}

# Each time the timer function is called, the 'timer.tmp' file's amount decreases by 1
timer() { 
    local runtime=$(cat "$root_path/data/tmp/timer.tmp")
    local timer
    local hoursTimer
    local minutesTimer
    local secondsTimer
    hoursTimer=$((runtime / 3600))
    minutesTimer=$(( (runtime % 3600) / 60 ))
    secondsTimer=$((runtime % 60))
    if [ "$hoursTimer" -lt 10 ];then
        hoursTimer="0$hoursTimer"
    fi
    if [ "$minutesTimer" -lt 10 ];then
        minutesTimer="0$minutesTimer"
    fi
    if [ "$secondsTimer" -lt 10 ];then
        secondsTimer="0$secondsTimer"
    fi
    timer="$hoursTimer : $minutesTimer : $secondsTimer"
    newTime=$((runtime - 1))
    echo "$newTime" > "$root_path/data/tmp/timer.tmp"
    echo "$timer"
}

uploader_box(){
    handleDialog '--colors --title "Upload The Results $(bash components/utils/getBatteryState.sh)" --no-shadow --yes-label "Yes" --no-label "No" --yesno "\n      \ZbAre you willing to upload the results now?\Zn\n\nIf yes, your connection will be checked and if not connected, you will be able to connect through our interface" 12 60'
		rc=$(cat $root_path/data/tmp/dialog_state.tmp)
    if [ $rc -ne 0 ]; then
        echo "user has DECLINED to upload the results" >> "$root_path/data/user_will.txt"
        handleDialog '--no-shadow --colors --ok-label " Continue " --msgbox "\Z0\Zb       The results are ready in a ZIP file.\Zn\n---------------------------------------------------\n\nYou can take a look at your results later or upload the ZIP file manually at \Zb'"$WEBSITE_URL"'\Zn" 10 55'
        menu_box_secondary
    else
        echo "user has ACCEPTED to upload the results" >> "$root_path/data/user_will.txt"
        check_internet_connection false
        local conn_status=$?
        if [ $conn_status -eq 1 ]; then
            handleDialog '--colors --no-shadow --msgbox "       \Zb\Z1No internet connection detected\Zn\n\n  Please hit \ZbEnter\Zn to open network manager" 8 50'
            nmtui-connect
            tput civis
            check_internet_connection false
            conn_status=$?
            if [ $conn_status -eq 1 ]; then
                menu_box_primary
            else
                uploader_progress_box
                local status=$?
                if [ $status -eq 1 ]; then
                    menu_box_primary
                elif [ $status -eq 2 ]; then
                    handleDialog '--no-shadow --colors --ok-label " Continue " --msgbox "                \Zb\Z1The upload failed!\Zn\n\n\Z0\Zb       The results are ready in a ZIP file.\Zn\n\n---------------------------------------------------\n\nYou can take a look at your results later or upload the ZIP file manually at \Zb'"$WEBSITE_URL"'\Zn" 13 55'
                    menu_box_secondary
                fi
            fi
        else
            uploader_progress_box
            local status=$?
            if [ $status -eq 1 ]; then
                menu_box_primary
            elif [ $status -eq 2 ]; then
                handleDialog '--no-shadow --colors --ok-label " Continue " --msgbox "                \Zb\Z1The upload failed!\Zn\n\n\Z0\Zb       The results are ready in a ZIP file.\Zn\n\n---------------------------------------------------\n\nYou can take a look at your results later or upload the ZIP file manually at \Zb'"$WEBSITE_URL"'\Zn" 13 55'
                menu_box_secondary
            fi
        fi
    fi
}

uploader_progress_box(){
    local percentage
    local didUploadSucceed
    local didResponseSucceed
    bash "$root_path/components/upload.sh" > /dev/null 2>&1 &
    (
    while true; do
        if [ -f "$root_path/data/tmp/uploadProgress.tmp" ]; then
            percentage=$(cat "$root_path/data/tmp/uploadProgress.tmp")
        else
            percentage="0"
        fi
        if [ -f "$root_path/data/tmp/didUploadSucceed.tmp" ]; then
            didUploadSucceed=$(cat "$root_path/data/tmp/didUploadSucceed.tmp")
        else
            didUploadSucceed="true"
        fi
        if [ -f "$root_path/data/tmp/didResponseSucceed.tmp" ]; then
            didResponseSucceed=$(cat "$root_path/data/tmp/didResponseSucceed.tmp")
        else
            didResponseSucceed="true"
        fi

        if [[ "$percentage" -ge 0 && "$percentage" -le 100 ]]; then
            echo "$percentage"
            echo "XXX"
            echo "Uploading Results..."
            echo "XXX"
        fi

        if [[ "$percentage" == "100" ]]; then
            sleep 1
            break
        fi
        if [ "$didUploadSucceed" = "false" ]; then
            break
        fi
        if [ "$didResponseSucceed" = "false" ]; then
            break
        fi
        sleep 1
    done
    ) | handleDialog '--gauge "Uploading Results ..." 7 40 0'
    tput civis
    didUploadSucceed=$(cat "$root_path/data/tmp/didUploadSucceed.tmp")
    didResponseSucceed=$(cat "$root_path/data/tmp/didResponseSucceed.tmp")
    if [[ "$didUploadSucceed" == "true" && "$didResponseSucceed" == "true" ]];then
        url=$(cat "$root_path/data/tmp/url.tmp")
        handleDialog '--no-shadow --colors --ok-label " Generate QR Code " --title "Thank You | '"$ARHE_VERSION"' $(bash components/utils/getBatteryState.sh)" --msgbox "\n\Z0\Zb                The Upload is done!\Zn\n---------------------------------------------------\n\nPlease hit Enter to generate a QR Code to access your token on our website. You can use the token to participate in our raffle. See https://flippyr.am for more information.\n\nNote: you should save or bookmark the url to access the token at any time later" 15 55'
        clear
        tput cnorm
        echo -e "\n"
        qrencode -t ANSI256 -s 1 "$url"
        echo -e "\nYou can now view your token. Scan the QR Code or Enter through this link: $url\n"
        echo -e "\nPress any key to close and continue ...\n"
        read -n 1 -s
        tput civis
        menu_box_secondary
    elif [[ "$didResponseSucceed" == "false" ]];then
        return 2
    else
        return 1
    fi
}

menu_box_primary(){
    local connection_status
    local connection_pid=$!
    local color
    local uploadButtonStatus

    while true; do
        tput civis
        check_internet_connection false
        if [ $? -eq 1 ]; then
            connection_status="Not Connected"
            color="\Z1"
            uploadButtonStatus="\Z1Deactivated\Zn"
        else
            connection_status="Connected"
            color="\Z2"
            uploadButtonStatus="\Z2Activated\Zn"
        fi

        handleDialog '--no-shadow --colors --nocancel --title "'"$ARHE_VERSION"' $(bash components/utils/getBatteryState.sh)" --menu "\n            Connection Status: \Zb'"$color$connection_status"'\Zn" 15 60 5 \
        1 "Check Network Connection" \
        2 "Upload The Results '"($uploadButtonStatus)"'" \
        3 "Network Manager" \
        4 "Open Terminal" \
        5 "Skip the upload" 2> "'"$root_path/data/tmp/menu_choice.txt"'"'
        
        menu_choice=$(<"$root_path/data/tmp/menu_choice.txt")

        case $menu_choice in
            1)
                # If the user clicks, network connection status is updated
                handleDialog '--no-shadow --infobox "Checking Network Connection ..." 6 40'
                sleep 1
                ;;
            2)
                if [ "$connection_status" = "Connected" ];then
                    uploader_progress_box
                    if [ $? -ne 0 ]; then
                        handleDialog '--no-shadow --colors --ok-label " Continue " --msgbox "                \Zb\Z1The upload failed!\Zn\n\n\Z0\Zb       The results are ready in a ZIP file.\Zn\n\n---------------------------------------------------\n\nYou can take a look at your results later or upload the ZIP file manually at \Zb'"$WEBSITE_URL"'\Zn" 13 55'
                        menu_box_secondary
                    fi
                fi
                ;;
            3)
                handleDialog '--no-shadow --infobox "Opening network manager..." 6 40'
                sleep 2
                nmtui-connect
                ;;
            4)
                clear
                tput cnorm
                echo -e "\e[35m\n ===============================================================================\e[0m"
                echo -e "\e[92m .----------------.  .----------------.  .----------------.  .----------------. 
    | .--------------. || .--------------. || .--------------. || .--------------. |
    | |      __      | || |  _______     | || |  ____  ____  | || |  _________   | |
    | |     /  \     | || | |_   __ \    | || | |_   ||   _| | || | |_   ___  |  | |
    | |    / /\ \    | || |   | |__) |   | || |   | |__| |   | || |   | |_  \_|  | |
    | |   / ____ \   | || |   |  __ /    | || |   |  __  |   | || |   |  _|  _   | |
    | | _/ /    \ \_ | || |  _| |  \ \_  | || |  _| |  | |_  | || |  _| |___/ |  | |
    | ||____|  |____|| || | |____| |___| | || | |____||____| | || | |_________|  | |
    | |              | || |              | || |              | || |              | |
    | '--------------' || '--------------' || '--------------' || '--------------' |
    '----------------'  '----------------'  '----------------'  '----------------' \e[0m"

                echo -e "\e[35m ______________________________________________________________________________
    |                                                                              |
    |                             \e[31mDropped Terminal\e[0m\e[35m                                 |
    |                                                                              |
    |              \e[92mPress \e[31mCtrl+D\e[0m\e[35m \e[92mor type \e[31m'exit'\e[0m\e[35m \e[92mto return to the menu.\e[0m\e[35m              |
    |______________________________________________________________________________|
                \e[0m"
                bash
                ;;
            5)
                handleDialog '--no-shadow --colors --ok-label " Continue " --msgbox "\Z0\Zb       The results are ready in a ZIP file.\Zn\n---------------------------------------------------\n\nYou can take a look at your results later or upload the ZIP file manually at \Zb'"$WEBSITE_URL"'\Zn" 10 55'
                menu_box_secondary
                ;;
        esac
    done
}

menu_box_secondary(){
    # TODO-Additional Feature: Place a button for participating in the study if user regrets his/her choice
    while true; do
        tput civis

        handleDialog '--no-shadow --nocancel --colors --title "'"$ARHE_VERSION"' $(bash components/utils/getBatteryState.sh)" --menu "\n\Z0\Zb         The experiment has ended!\Zn\n" 11 50 2 \
        1 "\ZbReboot\Zn" \
        2 "\ZbExit the UI\Zn" 2> "'"$root_path/data/tmp/menu_choice.txt"'"'
        
        menu_choice=$(<"$root_path/data/tmp/menu_choice.txt")
        
        case $menu_choice in
            1)
                bash "$root_path/components/cleanup.sh"
                
                if [ -f "/.dockerenv" ];then
                    tput cnorm
                    clear
                    exit 0
                fi
                reboot -f &
                sleep 5
                if ps -p $! > /dev/null; then
                    echo "Reboot failed; trying fallback methods..."
                    echo 1 > /proc/sys/kernel/sysrq
                    echo b > /proc/sysrq-trigger
                    echo c > /proc/sysrq-trigger
                fi
                ;;
            2)
                bash "$root_path/components/cleanup.sh"
                tput cnorm
                clear
                exit 0
                ;;
        esac
    done
}

user_agreements_box() {
    handleDialog '--colors --title "Disclaimer $(bash components/utils/getBatteryState.sh)" --no-shadow --yes-label "Accept" --no-label "Decline" --yesno "'"$(cat "$root_path/components/disclaimer.txt")"'" 20 70'
		rc=$(cat $root_path/data/tmp/dialog_state.tmp)
    if [ $rc -ne 0 ]; then
        echo "user has DECLINED the disclaimer" > "$root_path/data/user_will.txt"
        handleDialog '--colors --no-shadow --ok-label " Reboot " --msgbox "       \Zb\Z1You have declined the disclaimer\Zn\n\n       Thank you for your contribution!" 8 50'
        bash "$root_path/components/cleanup.sh"
        
        if [ -f "/.dockerenv" ];then
            tput cnorm
            clear
            exit 0
        fi
        reboot -f &
        sleep 5
        if ps -p $! > /dev/null; then
            echo "Reboot failed; trying fallback methods..."
            echo 1 > /proc/sys/kernel/sysrq
            echo b > /proc/sysrq-trigger
            echo c > /proc/sysrq-trigger
        fi
    else
        echo "user has ACCEPTED the disclaimer" > "$root_path/data/user_will.txt"
    fi
    handleDialog '--colors --title "Privacy Policy $(bash components/utils/getBatteryState.sh)" --no-shadow --yes-label "Accept" --no-label "Decline" --yesno "'"$(cat "$root_path/components/privacy.txt")"'" 20 70'
		rc=$(cat $root_path/data/tmp/dialog_state.tmp)
    if [ $rc -ne 0 ]; then
        echo "user has DECLINED the privacy policy" >> "$root_path/data/user_will.txt"
        handleDialog '--colors --no-shadow --ok-label " Reboot " --msgbox "     \Zb\Z1You have declined the privacy policy\Zn\n\n       Thank you for your contribution" 8 50'
        bash "$root_path/components/cleanup.sh"
        
        if [ -f "/.dockerenv" ];then
            tput cnorm
            clear
            exit 0
        fi
        reboot -f &
        sleep 5
        if ps -p $! > /dev/null; then
            echo "Reboot failed; trying fallback methods..."
            echo 1 > /proc/sys/kernel/sysrq
            echo b > /proc/sysrq-trigger
            echo c > /proc/sysrq-trigger
        fi
    else
        echo "user has ACCEPTED the privacy policy" >> "$root_path/data/user_will.txt"
    fi
}

compressor_box(){
    local percentage
    local isParticipated=$1
    bash "$root_path/components/compress.sh" > /dev/null 2>&1 &
    (
    while true; do
        if [ -f "$root_path/data/tmp/compressProgress.tmp" ]; then
            percentage=$(cat "$root_path/data/tmp/compressProgress.tmp")
        else
            percentage="0"
        fi

        if [[ "$percentage" -ge 0 && "$percentage" -le 100 ]]; then
            echo "$percentage"
            echo "XXX"
            echo "Compressing Results..."
            echo "XXX"
        fi

        if [ "$percentage" -eq 100 ]; then
            sleep 1
            break
        fi
        sleep 1
    done
    ) | handleDialog '--gauge "Compressing Results ..." 7 40 0'
    tput civis
    if [ "$isParticipated" = "false" ];then
        handleDialog '--no-shadow --colors --ok-label " Continue " --msgbox "\Z0\Zb   We have compressed the results to a ZIP file.\Zn\n---------------------------------------------------\n\nYou can take a look at your results later or upload the ZIP file manually at \Zb'"$WEBSITE_URL"'\Zn" 10 55'
        menu_box_secondary
    fi
}

study_participation_box() {
    handleDialog '--colors --title "Participation in study $(bash components/utils/getBatteryState.sh)" --no-shadow --yes-label "Yes" --no-label "No" --yesno "\n      \ZbAre you willing to participate in our study?\Zn" 7 60'
		rc=$(cat $root_path/data/tmp/dialog_state.tmp)
    if [ $rc -ne 0 ]; then
        echo "user has DECLINED to participate in the study" >> "$root_path/data/user_will.txt"
        compressor_box false
    else
        echo "user has ACCEPTED to participate in the study" >> "$root_path/data/user_will.txt"
        compressor_box true
        uploader_box
    fi
}

set_runtime() {
    tput cnorm
		# There is a bug with theform dialog when called multiple times, so this
		# Dialog will be shown normally without redraw (hope that is ok since
		# it should not be open too long anyway)
    dialog --no-shadow --colors --title "Set Experiment Duration $(bash components/utils/getBatteryState.sh)" --form "\nPress 'Tab' key on your keyboard to switch to the 'OK' button or simply press 'Enter'\n\nNavigate
    the input fields with the arrow keys ↑ ↓ and edit with the arrow keys ← →\n\n\Z1Default: 8 Hours\Zn" 15 60 0 \
        "Hours:" 1 1 "8" 1 10 10 0 \
        "Minutes:" 2 1 "0" 2 10 10 0 2> "$root_path/data/tmp/script_runtime.txt"
		if [ $? -ne 0 ]; then
				menu_box_secondary
		fi
    tput civis
    local hours=$(sed -n 1p "$root_path/data/tmp/script_runtime.txt")
    local minutes=$(sed -n 2p "$root_path/data/tmp/script_runtime.txt")
    local minRuntime
    local runtime_timestamp
    if ! [[ "$hours" =~ ^[0-9]+$ ]] || ! [[ "$minutes" =~ ^[0-9]+$ ]] && [[ -n $hours ]] && [[ -n $minutes ]]; then
        hours=""
        minutes=""
        local isNumeric="false"
    else
        local isNumeric="true"
        runtime_timestamp=$((hours * 3600 + minutes * 60))
        if [ "$runtime_timestamp" -lt "10800" ];then
            minRuntime="false"
        else
            minRuntime="true"
        fi
    fi

    while [[ -z $hours || -z $minutes || "$minRuntime" = "false" ]]
    do
        if [[ "$isNumeric" == "false" ]];then
            txt="\n\Z1ERROR: Only numbers are accepted as input! Please Try again.\Zn\n\nPress 'Tab' key on your keyboard to switch to the 'OK' button or simply press 'Enter'\n\nNavigate the input fields with the arrow keys ↑ ↓ and edit with the arrow keys ← →\n\n\Z1Min: 3 Hours\Zn"
        elif [[ "$minRuntime" == "false" && -n $minutes && -n $hours ]];then
            txt="\n\Z1ERROR: The minimum experiment runtime is 3 Hours! Please Try again.\Zn\n\nPress 'Tab' key on your keyboard to switch to the 'OK' button or simply press 'Enter'\n\nNavigate the input fields with the arrow keys ↑ ↓ and edit with the arrow keys ← →\n\n\Z1Min: 3 Hours\Zn"
        else
            txt="\n\Z1ERROR: The input fields cannot be empty! Please Try again.\Zn\n\nPress 'Tab' key on your keyboard to switch to the 'OK' button or simply press 'Enter'\n\nNavigate the input fields with the arrow keys ↑ ↓ and edit with the arrow keys ← →\n\n\Z1Min: 3 Hours\Zn"
        fi
        tput cnorm
				# There is a bug with theform dialog when called multiple times, so this
				# Dialog will be shown normally without redraw (hope that is ok since
				# it should not be open too long anyway)
        dialog --no-shadow --nocancel --colors --title "Set Experiment Duration $(bash components/utils/getBatteryState.sh)" --form "$txt" 17 60 0 \
            "Hours:" 1 1 "" 1 10 10 0 \
            "Minutes:" 2 1 "" 2 10 10 0 2> "$root_path/data/tmp/script_runtime.txt"
        tput civis
        hours=$(sed -n 1p "$root_path/data/tmp/script_runtime.txt")
        minutes=$(sed -n 2p "$root_path/data/tmp/script_runtime.txt")
        if ! [[ "$hours" =~ ^[0-9]+$ ]] || ! [[ "$minutes" =~ ^[0-9]+$ ]] && [[ -n $hours ]] && [[ -n $minutes ]]; then
            hours=""
            minutes=""
            isNumeric="false"
        else
            isNumeric="true"
            runtime_timestamp=$((hours * 3600 + minutes * 60))
            if [ "$runtime_timestamp" -lt "10800" ];then
                minRuntime="false"
            else
                minRuntime="true"
            fi
        fi
    done

    echo "$runtime_timestamp" > "$root_path/data/script_runtime.txt"
    echo "$runtime_timestamp" > "$root_path/data/tmp/timer.tmp"
}

progress_box() {
    local timer
    local stage1Progress
    local stage2Progress
    local stage3Progress
    local stage4Progress
    local stage1Color="\Z1"
    local stage2Color="\Z1"
    local stage3Color="\Z1"
    local stage4Color="\Z1"
    echo "0%" > "$root_path/data/tmp/stage1.tmp"
    echo "0%" > "$root_path/data/tmp/stage2.tmp"
    echo "0%" > "$root_path/data/tmp/stage3.tmp"
    echo "0%" > "$root_path/data/tmp/stage4.tmp"
    while true; do
        stage1Progress=$(cat "$root_path/data/tmp/stage1.tmp")
        stage2Progress=$(cat "$root_path/data/tmp/stage2.tmp")
        stage3Progress=$(cat "$root_path/data/tmp/stage3.tmp")
        stage4Progress=$(cat "$root_path/data/tmp/stage4.tmp")
        if [ "$stage1Progress" = "100%" ];then
            stage1Color="\Z2"
        fi
        if [ "$stage2Progress" = "100%" ];then
            stage2Color="\Z2"
        fi
        if [ "$stage3Progress" = "100%" ];then
            stage3Color="\Z2"
        fi
        if [ "$stage4Progress" = "100%" ];then
            stage4Color="\Z2"
        fi

        timer=$(timer)

				# Dialog is drawn any second anyway, so no refresh needed
        dialog --no-shadow --colors --title "Experimenting | $ARHE_VERSION $(bash components/utils/getBatteryState.sh)" --infobox "\n\nFetching Info (Stage 1): $stage1Color\Zb$stage1Progress\Zn\n\nRetrieving Addressing Functions (Stage 2): $stage2Color\Zb$stage2Progress\Zn\n\nVerifying & Injecting Functions (Stage 3): $stage3Color\Zb$stage3Progress\Zn\n\nHammering (Experiment - Last Stage): $stage4Color\Zb$stage4Progress\Zn\n\n--------------------------------------------------------\n\n        \Z4\ZbEstimated Remaining Time $timer\Zn" 16 60
        if [ "$stage4Progress" = "100%" ];then
            sleep 2
            break
        fi
        sleep 1
    done

    source conf.cfg
    blacksmithBitFlips="(Failed)"
    if [ "$(cat "$skipped_tools_log_path" | grep "Blacksmith" | grep -v -E "ended|failed")" != "" ]; then
        blacksmithBitFlips="(Skipped)"
    elif [ "$(cat "$skipped_tools_log_path" | grep "Blacksmith" | grep "failed")" != "" ]; then
        blacksmithBitFlips="(Failed)"
    elif [ -f "$root_path/data/tmp/totalBlacksmithBitFlips.tmp" ]; then
        blacksmithBitFlips=$(cat "$root_path/data/tmp/totalBlacksmithBitFlips.tmp")
    fi

    trrespassBitFlips="(Failed)"
    if [ "$(cat "$skipped_tools_log_path" | grep "TRRespass" | grep -v -E "ended|failed")" != "" ]; then
        trrespassBitFlips="(Skipped)"
    elif [ "$(cat "$skipped_tools_log_path" | grep "TRRespass" | grep "failed")" != "" ]; then
        trrespassBitFlips="(Failed)"
    elif [ -f "$root_path/data/tmp/hammersuiteBitFlipsNum.tmp" ]; then
        trrespassBitFlips=$(cat "$root_path/data/tmp/hammersuiteBitFlipsNum.tmp")
    fi

    flipfloydBitFlips="(Failed)"
    if [ "$(cat "$skipped_tools_log_path" | grep "FlipFloyd" | grep -v -E "ended|failed")" != "" ]; then
        flipfloydBitFlips="(Skipped)"
    elif [ "$(cat "$skipped_tools_log_path" | grep "FlipFloyd" | grep "failed")" != "" ]; then
        flipfloydBitFlips="(Failed)"
    elif [ -f "$root_path/data/tmp/flipfloydBitFlipsNum.tmp" ]; then
        flipfloydBitFlips=$(cat "$root_path/data/tmp/flipfloydBitFlipsNum.tmp")
    fi

    rowpressBitFlips="(Failed)"
    if [ "$(cat "$skipped_tools_log_path" | grep "RowPress" | grep -v -E "ended|failed")" != "" ]; then
        rowpressBitFlips="(Skipped)"
    elif [ "$(cat "$skipped_tools_log_path" | grep "RowPress" | grep "failed")" != "" ]; then
        rowpressBitFlips="(Failed)"
    elif [ -f "$root_path/data/tmp/rowpressBitFlipsNum.tmp" ]; then
        rowpressBitFlips=$(cat "$root_path/data/tmp/rowpressBitFlipsNum.tmp")
    fi

    rowhammerjsBitFlips="(Failed)"
    if [ "$(cat "$skipped_tools_log_path" | grep "RowhammerJs" | grep -v -E "ended|failed")" != "" ]; then
        rowhammerjsBitFlips="(Skipped)"
    elif [ "$(cat "$skipped_tools_log_path" | grep "RowhammerJs" | grep "failed")" != "" ]; then
        rowhammerjsBitFlips="(Failed)"
    elif [ -f "$root_path/data/tmp/rowhammerjsBitFlipsNum.tmp" ]; then
        rowhammerjsBitFlips=$(cat "$root_path/data/tmp/rowhammerjsBitFlipsNum.tmp")
    fi

    rowhammertestBitFlips="(Failed)"
    if [ "$(cat "$skipped_tools_log_path" | grep "Rowhammer_test" | grep -v -E "ended|failed")" != "" ]; then
        rowhammertestBitFlips="(Skipped)"
    elif [ "$(cat "$skipped_tools_log_path" | grep "Rowhammer_test" | grep "failed")" != "" ]; then
        rowhammertestBitFlips="(Failed)"
    elif [ -f "$root_path/data/tmp/rowhammertestBitFlipsNum.tmp" ]; then
        rowhammertestBitFlips=$(cat "$root_path/data/tmp/rowhammertestBitFlipsNum.tmp")
    fi

    hammertoolBitFlips="(Failed)"
    if [ "$(cat "$skipped_tools_log_path" | grep "HammerTool" | grep -v -E "ended|failed")" != "" ]; then
        hammertoolBitFlips="(Skipped)"
    elif [ "$(cat "$skipped_tools_log_path" | grep "HammerTool" | grep "failed")" != "" ]; then
        hammertoolBitFlips="(Failed)"
    elif [ -f "$root_path/data/tmp/hammertoolBitFlipsNum.tmp" ]; then
        hammertoolBitFlips=$(cat "$root_path/data/tmp/hammertoolBitFlipsNum.tmp")
    fi

    handleDialog '--no-shadow --colors --ok-label " Continue " \
    --title "Thank You | $ARHE_VERSION $(bash components/utils/getBatteryState.sh)" \
    --msgbox "\n\Z0\Zb             The Experiment is done!\Zn\n---------------------------------------------------\n\n\
    Blacksmith Bit Flips: \Z0\Zb'"$blacksmithBitFlips"'\Zn\n\n\
    TRRespass Bit Flips: \Z0\Zb'"$trrespassBitFlips"'\Zn\n\n\
    FlipFloyd Bit Flips: \Z0\Zb'"$flipfloydBitFlips"'\Zn\n\n\
    RowPress Bit Flips: \Z0\Zb'"$rowpressBitFlips"'\Zn\n\n\
    Rowhammer.Js Bit Flips: \Z0\Zb'"$rowhammerjsBitFlips"'\Zn\n\n\
    Rowhammer-test Bit Flips: \Z0\Zb'"$rowhammertestBitFlips"'\Zn\n\n\
    HammerTool Bit Flips: \Z0\Zb'"$hammertoolBitFlips"'\Zn" \
    22 55'

}

start_experiment() {
    user_agreements_box
    set_runtime
    bash ./main.sh -u > "$root_path/data/logs/main.log" 2>&1 &
    progress_box
    study_participation_box
}

start_experiment

tput cnorm
