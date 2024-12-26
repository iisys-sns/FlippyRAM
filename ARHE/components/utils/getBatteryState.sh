batteryPath="$(upower -e | grep /org/freedesktop/UPower/devices/battery_BAT | head -n1)"
if [ "$batteryPath" == "" ]; then
	echo -n ""
else
	echo -n "(battery $(upower -i "$batteryPath" | grep "percentage" | sed 's/ \+/ /g' | cut -d " " -f 3))"
fi
