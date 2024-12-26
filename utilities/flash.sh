#!/bin/bash

printUsage() {
	echo "Usage: $0 <DEVICE_SIZE> <IMAGE_NAME.iso>"
	exit -1
}

if [ "$#" -ne 2 ]; then
	printUsage
fi

DEVICE_SIZE="$1"
IMAGE="$2"

devices="$(lsblk | grep "$DEVICE_SIZE" | cut -d " " -f 1 | rev | sed 's/\(...\).*/\1/g' | rev | grep "sd[a-z]")"
pids=""

for device in $devices; do
		if [ "$(ls -l /dev/disk/by-id/usb-* | grep "$device")" == "" ]; then
			echo -e "\e[31mDevice $device is not a USB device. Skipping... \e[0m"
			continue
		fi

    echo -e "\e[92mStarting to Copy image to $device \e[0m"
    sudo dd if=$IMAGE of=/dev/$device > /dev/null 2>&1 &
		pid="$!"
		if [ "$pids" != "" ]; then
			pids="$pids "
		fi
		pids="${pids}${pid}"
done

for pid in $pids; do
    echo -e "\e[92mWaiting for dd PID $pid to finish... \e[0m"
		waitpid $pid
done

echo -e "\e[92mSyncing all devices... \e[0m"
sudo sync
echo -e "\e[92mDone flashing. \e[0m"
