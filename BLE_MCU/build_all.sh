#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_ROOT="${ROOT_DIR}/BLE_MCU"
BOOT_ROOT="${ROOT_DIR}/bootloader-uart-xmodem"

APP_CMAKE_DIR="${APP_ROOT}/cmake_gcc"
BOOT_CMAKE_DIR="${BOOT_ROOT}/cmake_gcc"

APP_BUILD_DIR="${APP_CMAKE_DIR}/build/base"
BOOT_BUILD_DIR="${BOOT_CMAKE_DIR}/build/base"

OUT_DIR="${ROOT_DIR}/out"
OUT_APP_DIR="${OUT_DIR}/app"
OUT_BOOT_DIR="${OUT_DIR}/bootloader"
OUT_PKG_DIR="${OUT_DIR}/package"

mkdir -p "${OUT_APP_DIR}" "${OUT_BOOT_DIR}" "${OUT_PKG_DIR}"

echo "== Step 1: Build bootloader =="
(
  cd "${BOOT_CMAKE_DIR}"
  cmake --workflow --preset project
)

echo "== Step 2: Build application =="
(
  cd "${APP_CMAKE_DIR}"
  cmake --workflow --preset project
)

echo "== Step 3: Generate GBL for application =="
"${ROOT_DIR}/connect_create_gbl_image.sh" "${APP_ROOT}"

echo "== Step 4: Collect artifacts =="
cp -f "${BOOT_BUILD_DIR}/bootloader-uart-xmodem."{elf,hex,s37,bin} "${OUT_BOOT_DIR}/" 2>/dev/null || true
cp -f "${APP_BUILD_DIR}/BLE_MCU."{elf,hex,s37,bin,gbl} "${OUT_APP_DIR}/" 2>/dev/null || true

echo "== Step 5: Optional combine image =="
if [[ -n "${PATH_SCMD:-}" ]]; then
  COMMANDER="${PATH_SCMD}/commander"

  if [[ -f "${BOOT_BUILD_DIR}/bootloader-uart-xmodem.hex" && -f "${APP_BUILD_DIR}/BLE_MCU.hex" ]]; then
    "${COMMANDER}" convert \
      "${BOOT_BUILD_DIR}/bootloader-uart-xmodem.hex" \
      "${APP_BUILD_DIR}/BLE_MCU.hex" \
      --outfile "${OUT_PKG_DIR}/combined.hex"
    echo "Combined image created: ${OUT_PKG_DIR}/combined.hex"
  else
    echo "Skip combine: required hex files not found."
  fi
else
  echo "Skip combine: PATH_SCMD is not set."
fi

echo "== Done =="
