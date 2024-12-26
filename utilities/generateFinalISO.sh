#!/bin/bash

showUsage() {
	echo "Usage: $0 <IN_ISO_IMAGE> <OUT_ISO_IMAGE> <LOOP_DEVICE>"
}

if [ "$#" -ne 3 ]; then
	showUsage
	exit -1
fi

if [ "$(mount | grep "/mnt")" != "" ]; then
	echo -e "\e[31mSomething is mounted at /mnt. Skipping ISO creation. \e[0m"
	exit -1
fi

IN_IMAGE="$1"
OUT_IMAGE="$2"
DEVICE="$3"

# Sizes in MiB without unit based on ISO image
ISO_PARTITION_START="1"
ISO_UNIT="$(ls -lh $IN_IMAGE | cut -d " " -f 5 | sed 's/^[^G^M]*\([GM]\)$/\1/g')"

echo "Unit: $ISO_UNIT"
if [ "$ISO_UNIT" == "M" ]; then
	# ISO Size in MiB
	ISO_SIZE="$(ls -lh $IN_IMAGE  | cut -d " " -f 5 | sed 's/^\([0-9]\+\)M$/\1/g')"
	ISO_PARTITION_END="$(echo "$(python -c "print(str(($ISO_SIZE) + 50))")" | cut -d "." -f 1)"
else
	# ISO Size in GiB, Multiply with 1024
	ISO_SIZE="$(ls -lh $IN_IMAGE  | cut -d " " -f 5 | sed 's/^\([0-9]\.[0-9]\)G$/\1/g')"
	ISO_PARTITION_END="$(echo "$(python -c "print(str(($ISO_SIZE * 2**10) + 50))")" | cut -d "." -f 1)"
fi
DATA_PARTITION_END="$(expr $ISO_PARTITION_END + 10)"
DATA_PARTITION_END_BYTES="$(python -c "print(str($DATA_PARTITION_END * 2**20))")"

echo "ISO_PARTITION_START = $ISO_PARTITION_START"
echo "ISO_SIZE = $ISO_SIZE"
echo "ISO_PARTITION_END = $ISO_PARTITION_END"
echo "DATA_PARTITION_END = $DATA_PARTITION_END"
echo "DATA_PARTITION_END_BYTES = $DATA_PARTITION_END_BYTES"

# Other vars
LO_CREATED=0

runCMD() {
	${1}
	if [ "$?" -ne "0" ]; then
		echo -e "\e[31m${2} Skipping ISO creation. \e[0m"
		echo -e "\e[36m[DEBUG]: Command: '${1}'\e[0m"
		if [ "$LO_CREATED" -eq "1" ]; then
			sudo losetup -d /dev/$DEVICE
		fi
		exit -1
	fi
}

echo -e "\e[92mCreating partitions for final ISO image...\e[0m"
runCMD "dd if=/dev/zero of=${OUT_IMAGE} bs=1M count=10" "Error writing zeroes to $OUT_IMAGE."
runCMD "truncate -s $(expr ${DATA_PARTITION_END_BYTES} + 512) ${OUT_IMAGE}" "Truncation of file $OUT_IMAGE failed."
runCMD "parted ${OUT_IMAGE} mklabel gpt" "Creation of Disk Label failed."
runCMD "parted ${OUT_IMAGE} mkpart primary fat32 ${ISO_PARTITION_START}MiB ${ISO_PARTITION_END}MiB" "Creation of first partition failed."
# Strange hack to use undocumented parted feature '---pretend-input-tty'
# to be able to pipe "yes" to the question if parted should automatically adjust
# the partition location as needed
echo 'yes' | parted ---pretend-input-tty ${OUT_IMAGE} mkpart primary ext4 ${ISO_PARTITION_END}MiB ${DATA_PARTITION_END}MiB
parted ${OUT_IMAGE} print
runCMD "sudo losetup -P /dev/${DEVICE} ${OUT_IMAGE}" "Creation of LOOP device failed."
LO_CREATED=1

echo -e "\e[92mCreating file systems for final ISO image...\e[0m"
runCMD "sudo mkfs.fat -F32 /dev/${DEVICE}p1" "Creation of file system on first partition failed."
runCMD "sudo mkfs.fat -F32 /dev/${DEVICE}p2" "Creation of file system on second partition failed."

echo -e "\e[92mMounting partitions and copying data for final ISO image...\e[0m"
runCMD "sudo mkdir /mnt/$DEVICE" "Creation of tmpdir for mount failed."
runCMD "sudo mount /dev/${DEVICE}p1 /mnt/$DEVICE" "Mount of first partition failed."
runCMD "sudo bsdtar -x -f $IN_IMAGE -C /mnt/$DEVICE" "Extracting of ISO image to first partition failed."
runCMD "sudo umount /mnt/$DEVICE" "UMount of first partition failed."
runCMD "sudo rm -r /mnt/$DEVICE" "Removal of tmpdir for mount failed."
runCMD "sudo syslinux --directory boot/syslinux --install /dev/${DEVICE}p1" "Installation of syslinux to first partition failed."
runCMD "sudo dd bs=440 count=1 conv=notrunc if=/usr/lib/syslinux/bios/gptmbr.bin of=/dev/$DEVICE" "Copy of MBR bootloader to first partition failed."
runCMD "sudo sync" "Sync failed."
tt=$(date +%Y%m)
runCMD "sudo dosfslabel /dev/${DEVICE}p1 ARCH_$tt" "Failed to set label for first partition."
runCMD "sudo sync" "Sync failed."
runCMD "sudo losetup -d /dev/$DEVICE" "Unable to cleanup loopback device."

echo -e "\e[92mImage $OUT_IMAGE created successful. \e[0m"
exit 0
