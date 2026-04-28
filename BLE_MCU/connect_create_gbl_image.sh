#!/bin/bash
set -euo pipefail

if [[ -n "${PATH_SCMD:-}" ]]; then
  COMMANDER="${PATH_SCMD}/commander"
else
  echo "Error: Please set PATH_SCMD environment variable first!"
  echo "Example: export PATH_SCMD=~/SimplicityStudio/v6/developer/adapter_packs/commander"
  exit 1
fi

PATH_PROJ="${1:-$(pwd)}"
IMAGE="${2:-${PATH_PROJ}/cmake_gcc/build/base/BLE_MCU.s37}"

if [[ ! -f "${IMAGE}" ]]; then
  echo "Error: app image not found: ${IMAGE}"
  echo "Build the BLE_MCU project first."
  exit 1
fi

IMAGE_NAME=$(basename "${IMAGE%.*}")
IMAGE_PATH=$(dirname "${IMAGE}")
IMAGE_EXT=${IMAGE##*.}

echo "Generating GBL from: ${IMAGE}"
"${COMMANDER}" gbl create "${IMAGE_PATH}/${IMAGE_NAME}.gbl" \
  --app "${IMAGE_PATH}/${IMAGE_NAME}.${IMAGE_EXT}" \
  --force

echo "GBL file created successfully: ${IMAGE_PATH}/${IMAGE_NAME}.gbl"
