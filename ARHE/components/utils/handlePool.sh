#!/bin/sh

source conf.cfg

root_path=$ROOT_PATH

cleanupFunctionList() {
  echo "$1" | tr ',' '\n' | sed 's/ \+//g' | sort | uniq | grep -v "^$" | tr '\n' ',' | sed 's/^\(.*\),$/\1/g'
}

# Read the addressing functions identified by the tool, convert them to the
# correct format (if required) and write them in a list
fns="$(cat $root_path/data/drama-functions.txt | grep "0x" | sed 's/.*\(0x[0-9a-f]\+\), Percentage.*$/\1/g' | tr '\n' ',' | sed 's/\(.*\),/\1/g')"
fns="$fns,$(cat $root_path/data/amdre-functions.txt)"
fns="$fns,$(cat $root_path/data/trrespass-functions.txt)"
fns="$fns,$(cat $root_path/data/dramdig-functions.txt)"
fns="$fns,$(cat $root_path/data/dare-functions.txt)"

# Sort and unify the list of all addressing functions and write it to the pool
# file which can be used for further testing
echo "$(cleanupFunctionList "$fns")" > $root_path/data/reaf-pool.txt

# Sort and unify the functions of all tools except drama (handled otherwise)
# (this might be required because sometimes there might be newlines, etc. in the original files)
echo "$(cleanupFunctionList "$(cat $root_path/data/amdre-functions.txt)")" > "$root_path/data/amdre-functions.txt"
echo "$(cleanupFunctionList "$(cat $root_path/data/trrespass-functions.txt)")" > "$root_path/data/trrespass-functions.txt"
echo "$(cleanupFunctionList "$(cat $root_path/data/dramdig-functions.txt)")" > "$root_path/data/dramdig-functions.txt"
echo "$(cleanupFunctionList "$(cat $root_path/data/dare-functions.txt)")" > "$root_path/data/dare-functions.txt"
