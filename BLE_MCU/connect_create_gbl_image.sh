#!/bin/bash

# use PATH_SCMD env var to set path for Simplicity Commander
if [[ -n ${PATH_SCMD} ]]; then
  COMMANDER="${PATH_SCMD}/commander"
else
  echo "Error: Please set PATH_SCMD environment variable first!"
  echo "Example: export PATH_SCMD=~/SimplicityStudio/v6/developer/adapter_packs/commander"
  read -p "Press enter to continue..."
  exit 1
fi

# project path
PATH_PROJ="$1"

if [ -z "$PATH_PROJ" ]; then
  echo "No project path specified. Using current directory."
  PATH_PROJ=$(pwd)
fi
cd "$PATH_PROJ" || exit 1

# 查找s37文件（仅第一个，避免多文件冲突）
IMAGE=$(find . -type f -iname "*.s37" | head -n 1)

if [[ -z "$IMAGE" || ! -f "$IMAGE" ]]; then
  echo "Error: no firmware image (.s37) found"
  echo "Was the project compiled and linked successfully?"
  read -p "Press enter to continue..."
  exit 1
fi

# 拆分文件名（关键：重命名PATH为IMAGE_PATH，不破坏系统变量）
IMAGE_NAME=$(basename "${IMAGE%.*}")
IMAGE_PATH=$(dirname "$IMAGE")
IMAGE_EXT=${IMAGE##*.}

# 生成GBL文件
echo "Generating GBL from: $IMAGE"
"$COMMANDER" gbl create "${IMAGE_PATH}/${IMAGE_NAME}.gbl" --app "${IMAGE_PATH}/${IMAGE_NAME}.${IMAGE_EXT}" --force

echo "GBL file created successfully: ${IMAGE_PATH}/${IMAGE_NAME}.gbl"
read -p "Press enter to continue..."

