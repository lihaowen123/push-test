#!/bin/bash

# .\scripts\sync_web.sh release-v6.0.3 /path/to/Community
web_tag_file="$(pwd)/web_sync_tag"

if [ -z "$1" ]; then
    web_branch="release-v6.0.3"
else
    web_branch="$1"
fi

if [ -z "$2" ]; then
    web_root="$(pwd)/../Community/"
else
    web_root="$2"
fi

# if [ -z "$3" ]; then
#     cp_branch="feature/syncweb"
# else
#     cp_tmpval="$3"
#     cp_branch="${cp_tmpval#origin/}"
# fi

cp_source="$(pwd)"
echo "web_root=$web_root"
echo "cp_source=$cp_source"

if [ ! -d "$web_root" ]; then
    echo "$web_root not exist"
    exit 1
fi

# 定义源目录和目标目录
sync_source_dir="$web_root/Community/dist"
sync_target_dir="$cp_source/resources/web/homepage"

# 读取 web repo 版本
if [ -f "$web_tag_file" ]; then
    WEB_TAG_NUM=$(<"$web_tag_file")
else
    WEB_TAG_NUM=0
fi

echo "CP_RESET"
cd "$cp_source" || exit
# reset hard
echo "reset hard cp"
git reset --hard

# web repo start
echo "WEB_REPO start"
echo "curwebdir=$web_root"
cd "$web_root" || exit
git fetch
git checkout "$web_branch"
git reset --hard
git pull origin "$web_branch"

# 获取 git rev-list 的输出
GIT_COUNT=$(git rev-list "$web_branch" --count)
echo "Git count: $GIT_COUNT"
echo "Web tag num: $WEB_TAG_NUM"

if [ "$GIT_COUNT" -eq "$WEB_TAG_NUM" ]; then
    echo "Count is equal to $WEB_TAG_NUM, need not to run web build."
    SYNC_WEB=true
else
    echo "Count is not equal to $WEB_TAG_NUM"
    SYNC_WEB=false
fi

if [ "$SYNC_WEB" = false ]; then
    # run web build
    cd "$web_root/Community" || exit
    rm -rf "$sync_source_dir"
    npm run release || exit 1
    echo "npm run release end"
fi

# copy start
echo "SYNC_WEB Start"
echo "$GIT_COUNT" > "$web_tag_file"

if [ ! -d "$sync_source_dir" ]; then
    echo "$sync_source_dir not exist"
    exit 1
fi

echo "copy start"
cp -R "$sync_source_dir/"* "$sync_target_dir/"
echo "copy end"

