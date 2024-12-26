#!/bin/bash
DEFAULT_INITIAL_PARTSIZE="10.5MB"

if [ -d "/arhe" ]; then
	echo -e "\e[31m The directory '/arhe' does already exist. Another instance of the experiment might be running (or did finish) without rebooting the system. Please reboot to start the experiment again (or continue while the experiment is still running). \e[0m"
	exit -1
fi

devices="$(ls -l /dev/disk/by-id/usb-* | cut -d ">" -f 2 | sed 's/.*\(sd[a-z][0-9]*\)$/\1/g')"
valid_device=""
for device in $devices; do
	if [[ "$devices" =~ "${device}1" ]] && [[ "$devices" =~ "${device}2" ]]; then
		if [ "$valid_device" != "" ]; then
			echo -e "\e[31m Identification of the USB drive failed. Multiple USB devices with the correct number of partitions detected. Please remove other USB drives and reboot. \e[0m"
			exit -1
		else
			valid_device="$device"
		fi
	fi
done
if [ "$valid_device" == "" ]; then
	echo -e "\e[31m Identification of the USB drive failed. No USB devices with the correct number of partitions detected. Are you booting the ISO from a USB drive? \e[0m"
	exit -1
fi

initialSize="$(parted -s /dev/${valid_device} print | grep "^ 2" | sed 's/ \+/ /g' | cut -d " " -f 5)"
echo "[DEBUG]: Initial Size: $initialSize"

if [ "$initialSize" == "$DEFAULT_INITIAL_PARTSIZE" ]; then
	echo -e "\e[92m Resizing the USB data partition ... \e[0m"
	# Resize (initially very small second partition to 100%)
	parted -s -f /dev/${valid_device} resizepart 2 100%
	if [ "$?" -ne "0" ]; then
		echo -e "\e[31m Unable to resize the USB data partition ... \e[0m"
		exit -1
	fi
	mkfs.fat -F32 -I /dev/${valid_device}2
	if [ "$?" -ne "0" ]; then
		echo -e "\e[31m Unable to extend the file system of the USB data partition ... \e[0m"
		exit -1
	fi

else
	echo -e "\e[92m Not required to resize the USB data partition (was already resized before) \e[0m"
fi

echo -e "\e[92m Mounting the USB data partition ... \e[0m"

mkdir -p /arhe

# Check if the partition is already mounted
if ! mountpoint -q /arhe; then
    mount /dev/${valid_device}2 /arhe
else
    echo -e "\e[92m The partition is already mounted at /arhe. \e[0m"
fi

# Ensure /arhe/ARHE exists before cleaning up
mkdir -p /arhe/ARHE

# Remove all files in /arhe/ARHE except .zip files and tokens.txt
find /arhe/ARHE -type f ! \( -name "*.zip" -o -name "tokens.txt" \) -delete
# Remove any empty directories in /arhe/ARHE
find /arhe/ARHE -type d -empty -delete

cp -r /root/ARHE /arhe

echo -e "\e[92m The script successfully mounted on the USB partition. Executing test... \e[0m"

cd /arhe/ARHE
timedatectl set-local-rtc false
timedatectl set-ntp true
/bin/bash run.sh

sleep 10

