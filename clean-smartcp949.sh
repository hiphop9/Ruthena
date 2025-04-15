#!/bin/bash

# 입력 파일 확장자 확인 (Git은 확장자 정보를 넘기지 않음. 경로로 처리 불가. 대신 확장자 감지 코드 삽입 가능)
# 우리는 확장자를 추론할 수 없으므로 첫 줄 기준으로 주석 유형을 판단하거나, 또는 내부 확장자 판별 로직을 삽입해야 함.
# 하지만 Git에서는 clean 필터가 확장자를 직접 넘기지 않기 때문에, 우회적으로 스크립트를 나누거나 자동 감지 방식이 필요함.

# 해결 방법: 입력을 임시 파일로 받고, 확장자 감지 및 주석 형태 지정
tmp_in=$(mktemp)
cat > "$tmp_in"

# 확장자 추론 불가하므로, 우선 입력 파일 내용으로 주석 유형을 감지하거나 디폴트를 적용
# 대안: .gitattributes에서 확장자별로 별도 필터를 사용하도록 분기 처리 권장

# 첫 줄 읽기
firstline=$(head -n 1 "$tmp_in")

# 기본 주석
comment="// 한글 인코딩을 위한 cp949"

# 첫 줄에 주석이 이미 있는지 판단
if echo "$firstline" | grep -q "한글 인코딩을 위한 cp949"; then
    # 그대로 변환
    iconv -f utf-8 -t cp949 "$tmp_in"
    rm "$tmp_in"
    exit 0
fi

# 확장자에 따라 주석 유형 변경 (우리는 확장자 정보 없음 -> 추후 확장자별 스크립트 분기 추천)
# 대안: .gitattributes에서 filter를 확장자별로 다르게 지정

# 디폴트는 `//`, yml인 경우는 `#`
# 여기서 확장자 구분이 안 되므로 .gitattributes를 이렇게 쓰는 게 제일 낫습니다:
# *.txt    filter=smartcp949_txt
# *.conf   filter=smartcp949_txt
# *.yml    filter=smartcp949_yml
# *.yaml   filter=smartcp949_yml

---

## ? 추천 방식: 확장자별로 filter 분리

### `.gitattributes` (확장자별 필터)

```gitattributes
*.txt    filter=smartcp949_txt
*.conf   filter=smartcp949_txt
*.yml    filter=smartcp949_yml
*.yaml   filter=smartcp949_yml
