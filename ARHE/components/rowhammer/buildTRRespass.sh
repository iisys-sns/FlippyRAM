#!/bin/bash

source conf.cfg

root_path=$ROOT_PATH
hammersuiteTpl="$root_path/Tools/hammersuite/hammersuiteTpl.c"
hammersuiteFile="$root_path/Tools/hammersuite/src/main.c"
tmpC="$root_path/data/tmp/tmp_c.c"
generatedSnippetC="$root_path/data/tmp/generated_snippet_hammersuite.c"
tmpBool="$root_path/data/tmp/injector.tmp"

echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} Initiating Addressing Functions Injection to Hammersuite ..."

if [[ -f "$root_path/data/hammersuiteDefFns.txt" ]]; then
    def_fns=$(cat "$root_path/data/hammersuiteDefFns.txt")
else
    def_fns="false"
fi

if [[ "$def_fns" == "true" ]]; then
    python3 "$root_path/components/rowhammer/buildTRRespass.py" -d
else
    python3 "$root_path/components/rowhammer/buildTRRespass.py"
fi

start_marker="/* START_DYNAMIC_INJECTION */"
end_marker="/* END_DYNAMIC_INJECTION */"

> "$tmpC"

echo "false" > "$tmpBool"

while IFS= read -r line
do
    if [[ "$line" == "$start_marker" ]]; then
        echo "true" > "$tmpBool"
        echo "$line" >> "$tmpC"
        cat "$generatedSnippetC" >> "$tmpC"
    elif [[ "$line" == "$end_marker" ]]; then
        echo "false" > "$tmpBool"
    fi

    cond=$(cat "$tmpBool")

    if [[ "$cond" == false ]]; then
        if [[ "$line" == "$end_marker" ]]; then
            echo -e "\n$line" >> "$tmpC"
        else
            echo "$line" >> "$tmpC"
        fi
    fi
done < "$hammersuiteTpl"

mv "$tmpC" "$hammersuiteFile"

echo -e "${BLACK_TXT}${PURPLE_BG} Rowhammer ${COLOR_RESET} ${GREEN_TXT}Addressing Functions Injected Successfully!${COLOR_RESET}"
