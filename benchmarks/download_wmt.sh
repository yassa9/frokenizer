#!/bin/bash
set -e

TARGET_FILE=$1
URL=${2:-"https://www.statmt.org/wmt14/training-monolingual-news-crawl/news.2013.en.shuffled.gz"}
TEMP_DOWNLOAD="data/temp_download"

if [ -f "$TARGET_FILE" ]; then
    echo "[*] Benchmark corpus already exists. Skipping."
    exit 0
fi

echo "[*] Downloading: $URL"
wget -c "$URL" -O "$TEMP_DOWNLOAD"

# check if the downloaded file is actually a gzip file
if file "$TEMP_DOWNLOAD" | grep -q 'gzip compressed data'; then
    echo "[*] Detected GZIP format. Extracting ..."
    gunzip -c "$TEMP_DOWNLOAD" > "$TARGET_FILE"
    rm "$TEMP_DOWNLOAD"
else
    echo "[*] File arrived already unzipped. Moving to target."
    mv "$TEMP_DOWNLOAD" "$TARGET_FILE"
fi

echo "[*] Ready: $TARGET_FILE ($(du -h "$TARGET_FILE" | cut -f1))"
