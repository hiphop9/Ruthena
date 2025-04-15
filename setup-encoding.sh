#!/bin/bash

echo "[설정 시작] .git/config에 인코딩 필터 추가 중..."

# 필터 설정 추가
git_config=".git/config"

# smartcp949_txt 필터 추가
if ! grep -q "\[filter \"smartcp949_txt\"\]" "$git_config"; then
    cat <<EOF >> "$git_config"

[filter "smartcp949_txt"]
    clean = ./clean-cp949-txt.sh
    smudge = iconv -f cp949 -t utf-8
EOF
    echo "? smartcp949_txt 필터 추가됨"
else
    echo "?? smartcp949_txt 필터는 이미 존재함"
fi

# smartcp949_yml 필터 추가
if ! grep -q "\[filter \"smartcp949_yml\"\]" "$git_config"; then
    cat <<EOF >> "$git_config"

[filter "smartcp949_yml"]
    clean = ./clean-cp949-yml.sh
    smudge = iconv -f cp949 -t utf-8
EOF
    echo "? smartcp949_yml 필터 추가됨"
else
    echo "?? smartcp949_yml 필터는 이미 존재함"
fi

# 스크립트 실행 권한 부여
chmod +x clean-cp949-txt.sh
chmod +x clean-cp949-yml.sh

echo "[완료] Git 필터 설정 및 스크립트 권한 설정 완료!"
