#!/bin/sh
# This is a wrapper for decode-dimms. Since the eeprom module was removed from
# the kernel, and other similar modules require manual setup (e.g. manually
# registering the required addresses with the drivers), decode-dimm did not work
# anymore.
#
# This wrapper script registers the required addresses with the driver ee1004
# and runs decode-dimms afterwards. Finally, it removes all these addresses from
# that driver again to restore the initial state.

myPath="$(echo "${BASH_SOURCE[0]}" | rev | cut -d "/" -f 2- | rev)"
source $myPath/../../conf.cfg

root_path="$ROOT_PATH"
log_file="$root_path/data/logs/decode-dimms.log"

while getopts "n" opt; do
  case $opt in
    n)
      isFlag="true"
      ;;
    *)
      echo "Usage: $0 [-n] | Do not use this flag | Only used for internal handling"
      exit 1
      ;;
  esac
done

if [ "$isFlag" = "true" ];then
  modprobe i2c-dev
  modprobe ee1004

  busses="$(i2cdetect -l | grep "^[^ ]* *smbus.*$" | tr '\t' ' ' | cut -d " " -f1)"
  addrs="0x50 0x51 0x52 0x53 0x54 0x55 0x56 0x57"

  for bus in $busses; do
    busId="$(echo "$bus" | sed 's/^i2c-\([0-9]\+\)/\1/g')"
    for addr in $addrs; do
      addrNoPrefix="$(echo "$addr" | sed 's/^0x\([0-9]\+\)/\1/g')"
      echo "ee1004 $addr" > /sys/bus/i2c/devices/$bus/new_device > "$log_file" 2>&1
      output="$(hexdump /sys/bus/i2c/drivers/ee1004/$busId-00$addrNoPrefix/eeprom 2>&1 | head -n1)"
      if [[ "$output" =~ "No such file or directory" ]]; then
        echo "$addr" > /sys/bus/i2c/devices/$bus/delete_device > "$log_file" 2>&1
        continue
      fi

      if [[ "$output" =~ "No such device or address" ]]; then
        echo "$addr" > /sys/bus/i2c/devices/$bus/delete_device > "$log_file" 2>&1
        continue
      fi
    done
  done

  decode-dimms

  for bus in $busses; do
    for addr in $addrs; do
      echo "$addr" > /sys/bus/i2c/devices/$bus/delete_device > "$log_file" 2>&1
    done
  done
else
  modprobe i2c-dev
  modprobe ee1004

  busses="$(i2cdetect -l | grep "^[^ ]* *smbus.*$" | tr '\t' ' ' | cut -d " " -f1)"
  addrs="0x50 0x51 0x52 0x53 0x54 0x55 0x56 0x57"

  for bus in $busses; do
    busId="$(echo "$bus" | sed 's/^i2c-\([0-9]\+\)/\1/g')"
    for addr in $addrs; do
      addrNoPrefix="$(echo "$addr" | sed 's/^0x//')"
      device_path="/sys/bus/i2c/drivers/ee1004/$busId-00$addrNoPrefix/eeprom"

      # Check if the device is already registered
      if [ -e "$device_path" ]; then
        echo "Device $addr on bus $bus is already registered." > "$log_file"
        continue
      fi

      # Attempt to register the device
      echo "ee1004 $addr" > /sys/bus/i2c/devices/$bus/new_device > "$log_file" 2>&1

      # Verify if the device was registered successfully
      if [ ! -e "$device_path" ]; then
        echo "Failed to register device $addr on bus $bus." > "$log_file"
        continue
      fi

      output="$(hexdump "$device_path" 2>&1 | head -n1)"
      if [[ "$output" =~ "No such file or directory" || "$output" =~ "No such device or address" ]]; then
        echo "$addr" > /sys/bus/i2c/devices/$bus/delete_device > "$log_file" 2>&1
      fi
    done
  done

  decode-dimms

  for bus in $busses; do
    for addr in $addrs; do
      addrNoPrefix="$(echo "$addr" | sed 's/^0x//')"
      device_path="/sys/bus/i2c/drivers/ee1004/$busId-00$addrNoPrefix/eeprom"

      if [ -e "$device_path" ]; then
        echo "$addr" > /sys/bus/i2c/devices/$bus/delete_device > "$log_file" 2>&1
      fi
    done
  done
fi

exit 0
