start_time=$(date +%s)
echo "start time: ${start_time}"
taskset 0x1 ./dramdig 
end_time=$(date +%s)
echo "end time: ${end_time}"
echo "[+] costs: $(($end_time-$start_time)) sec."
