#!/bin/sh

source conf.cfg

root_path=$ROOT_PATH
infile="$root_path/data/reaf-pool-verified.txt"
outfile="$root_path/data/final-functions.txt"
nfns=$(cat "$root_path/data/functions-num.txt")

cat "$infile" | sed 's/^.*Function //g; s/ has a dif[^0-9]*/ /g' | cut -d " " -f 1-2 | sort -k 2 | head -n $nfns | cut -d " " -f 1 | tr '\n' ',' | sed 's/\(.*\),$/\1/g' > "$outfile"
