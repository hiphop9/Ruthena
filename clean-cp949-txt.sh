#!/bin/bash

tmpfile=$(mktemp)
read -r firstline < /dev/stdin

if echo "$firstline" | grep -q "한글 인코딩을 위한 cp949"; then
    echo "$firstline" > "$tmpfile"
    cat >> "$tmpfile"
else
    echo "// 한글 인코딩을 위한 cp949" > "$tmpfile"
    echo "$firstline" >> "$tmpfile"
    cat >> "$tmpfile"
fi

iconv -f utf-8 -t cp949 "$tmpfile"
rm "$tmpfile"