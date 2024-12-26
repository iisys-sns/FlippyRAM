#!/bin/bash

source conf.cfg

root_path=$ROOT_PATH
mappingTpl="$root_path/Tools/RowPress/MappingTpl.h"
mappingFile="$root_path/Tools/RowPress/Mapping.h"
tmpH="$root_path/data/tmp/tmp_H.h"
generatedSnippetH="$root_path/data/tmp/generated_snippet_rowpress.h"
tmpBool="$root_path/data/tmp/injector.tmp"
buildLogFile="$root_path/data/logs/build.log"

echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} Initiating Addressing Functions Injection to RowPress ..."

python3 "$root_path/components/rowhammer/buildRowPress.py"

if [ $? -ne 0 ]; then
    echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} Error generating C++ code snippet"
    exit 1
fi

start_marker="/* START_DYNAMIC_INJECTION */"
end_marker="/* END_DYNAMIC_INJECTION */"

> "$tmpH"

echo "false" > "$tmpBool"

while IFS= read -r line
do
    if [[ "$line" == "$start_marker" ]]; then
        echo "true" > "$tmpBool"
        echo "$line" >> "$tmpH"
        cat "$generatedSnippetH" >> "$tmpH"
    elif [[ "$line" == "$end_marker" ]]; then
        echo "false" > "$tmpBool"
    fi

    cond=$(cat "$tmpBool")

    if [[ "$cond" == false ]]; then
        if [[ "$line" == "$end_marker" ]]; then
            echo -e "\n$line" >> "$tmpH"
        else
            echo "$line" >> "$tmpH"
        fi
    fi
done < "$mappingTpl"

mv "$tmpH" "$mappingFile"

echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} ${GREEN_TXT}Addressing Functions Injected Successfully!${COLOR_RESET}"

echo "Start building RowPress | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogFile"
cd "$root_path/Tools/RowPress"
make >> "$buildLogFile" 2>&1
if [ $? -ne 0 ]; then
	echo "Failed building RowPress | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogFile"
    cd "$root_path"
	exit 1
fi
cd "$root_path"
echo "Done building RowPress | $(date +%Y-%m-%d_%H-%M-%S)" >> "$buildLogFile"