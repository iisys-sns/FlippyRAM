#!/bin/bash

source conf.cfg

root_path=$ROOT_PATH
DRAMAddrTpl="$root_path/Tools/blacksmith/DRAMAddrTpl.cpp"
DRAMAddrFile="$root_path/Tools/blacksmith/src/Memory/DRAMAddr.cpp"
GlobalDefinesTpl="$root_path/Tools/blacksmith/GlobalDefinesTpl.hpp"
GlobalDefinesFile="$root_path/Tools/blacksmith/include/GlobalDefines.hpp"
tmpCpp="$root_path/data/tmp/tmp_cpp.cpp"
tmpHpp="$root_path/data/tmp/tmp_hpp.hpp"
generatedSnippetCpp="$root_path/data/tmp/generated_snippet_blacksmith.cpp"
generatedSnippetHpp="$root_path/data/tmp/generated_snippet_blacksmith.hpp"
tmpBool="$root_path/data/tmp/injector.tmp"

echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} Initiating Addressing Functions Injection to Blacksmith ..."

python3 "$root_path/components/rowhammer/buildBlacksmith.py"

if [ $? -ne 0 ]; then
    echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} Error generating C++ code snippet"
    exit 1
fi

start_marker="/* START_DYNAMIC_INJECTION */"
end_marker="/* END_DYNAMIC_INJECTION */"

> "$tmpCpp"

echo "false" > "$tmpBool"

while IFS= read -r line
do
    if [[ "$line" == "$start_marker" ]]; then
        echo "true" > "$tmpBool"
        echo "$line" >> "$tmpCpp"
        cat "$generatedSnippetCpp" >> "$tmpCpp"
    elif [[ "$line" == "$end_marker" ]]; then
        echo "false" > "$tmpBool"
    fi

    cond=$(cat "$tmpBool")

    if [[ "$cond" == false ]]; then
        if [[ "$line" == "$end_marker" ]]; then
            echo -e "\n$line" >> "$tmpCpp"
        else
            echo "$line" >> "$tmpCpp"
        fi
    fi
done < "$DRAMAddrTpl"

mv "$tmpCpp" "$DRAMAddrFile"

> "$tmpHpp"

echo "false" > "$tmpBool"

while IFS= read -r line
do
    if [[ "$line" == "$start_marker" ]]; then
        echo "true" > "$tmpBool"
        echo "$line" >> "$tmpHpp"
        cat "$generatedSnippetHpp" >> "$tmpHpp"
    elif [[ "$line" == "$end_marker" ]]; then
        echo "false" > "$tmpBool"
    fi

    cond=$(cat "$tmpBool")

    if [[ "$cond" == false ]]; then
        if [[ "$line" == "$end_marker" ]]; then
            echo -e "\n$line" >> "$tmpHpp"
        else
            echo "$line" >> "$tmpHpp"
        fi
    fi
done < "$GlobalDefinesTpl"

mv "$tmpHpp" "$GlobalDefinesFile"

echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} ${GREEN_TXT}Addressing Functions Injected Successfully!${COLOR_RESET}"
