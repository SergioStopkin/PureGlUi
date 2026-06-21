#!/usr/bin/env bash
. .cicd-config

echo $SOURCE_FILES $TEST_FILES | xargs -n 1 clang-format$CLANG_VERSION -i

echo $CMAKE_FILES | xargs -n 1 cmake-format -i

for file in $JSON_FILES; do
    jq . "$file" > "$file.tmp" && mv "$file.tmp" "$file"
done

[ -e "./.git" ] && git -C "./" status
