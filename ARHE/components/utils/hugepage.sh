#!/bin/bash

while getopts "m" opt; do
  case $opt in
    m)
      isFlag="true"
      ;;
    *)
      echo "Usage: $0 [-m] | Allocate 2 hugepages"
      exit 1
      ;;
  esac
done

gbFile="/sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages"
nPages=$(cat $gbFile)
if [ "$?" -ne 0 ]; then
	echo "Unable to access page file."
	exit 1
fi

if [ "$nPages" -eq 0 ]; then
	# Try to allocate hugepage since no hugepage is allocated at the moment
	if [ "$isFlag" = "true" ];then
		echo 2 > $gbFile 
		if [ "$(cat $gbFile)" -lt 2 ]; then
			echo "Unable to allocate hugepage."
			exit 1
		fi
	else
		echo 1 > $gbFile 
		if [ "$(cat $gbFile)" -eq 0 ]; then
			echo "Unable to allocate hugepage."
			exit 1
		fi
	fi
fi

if [ "$(mount | grep "/mnt/huge")" == "" ]; then
	# Try to mount hugepage since no hugepage is mounted at the moment
	mkdir -p /mnt/huge
	mount -t hugetlbfs  -o pagesize=1G none /mnt/huge
	if [ $? -ne 0 ]; then
		echo "Unable to mount hugepage."
		exit 1
	fi
fi
