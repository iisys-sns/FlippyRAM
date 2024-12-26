#!/bin/bash

source conf.cfg
root_path=$ROOT_PATH
REMOTE_DIR="/home/scpuser/ARHE"
log_file="$root_path/data/logs/upload_script.log"
fileNamePath="$root_path/data/tmp/zipFileName.tmp"
fileName=$(cat "$fileNamePath")
uploadProgress="$root_path/data/tmp/uploadProgress.tmp"
urlPath="$root_path/data/tmp/url.tmp"
didUploadSucceed="$root_path/data/tmp/didUploadSucceed.tmp"
didResponseSucceed="$root_path/data/tmp/didResponseSucceed.tmp"
echo "true" > "$didUploadSucceed"
echo "true" > "$didResponseSucceed"
echo "0" > "$uploadProgress"

fileName="$root_path/$fileName"

systemctl start NetworkManager.service > /dev/null 2>&1

while getopts "q" opt; do
  case $opt in
    q)
      isFlag="true"
      ;;
    *)
      echo "Usage: $0 [-q] | Do not use this flag | Only used for internal handling"
      exit 1
      ;;
  esac
done

/usr/bin/curl -i -F "file=@$fileName" "https://$WEBSITE_URL/upload/" 2>&1 | tee "$log_file"
upload_status=$?
res=$(cat "$log_file" )
if [ $upload_status -ne 0 ]; then
    echo "------------------------------------------" >> "$log_file"
    echo "Upload to server failed (Curl Failed)" >> "$log_file"
    echo "false" > "$didUploadSucceed"
    exit 1
else
    echo "true" > "$didUploadSucceed"
fi
echo "------------------------------------------" >> "$log_file"

location=$(echo "$res" | grep -i '^location:' | cut -d ' ' -f2)

if [ -z "$location" ]; then
    echo "Location is empty" >> "$log_file"
    echo "false" > "$didResponseSucceed"
    exit 1
fi
echo "Location: $location" >> "$log_file"

hash=$(echo "$location" | grep -oP '/token/\K[0-9a-f]{64}')

if [ -z "$hash" ]; then
    echo "hash is empty" >> "$log_file"
    echo "false" > "$didResponseSucceed"
    exit 1
fi
echo "Hash: $hash" >> "$log_file"

echo "$hash" >> "$root_path/tokens.txt"

echo "100" > "$uploadProgress"

url="${WEBSITE_URL}${location}"
echo "$url" > "$urlPath"

echo "url: $url" >> "$log_file"

if [ "$isFlag" = "true" ];then
    echo -e "\n"
    qrencode -t ANSI256 -s 1 "$url"
    echo -e "\nYou can now view your results. Scan the QR Code or Enter through this link: $url\n"
    bash "$root_path/components/cleanup.sh"
fi