#!/usr/bin/env bash
# Host static analysis for the portable C core.
# Requires cppcheck and clang-tidy on PATH. The STM32 port is excluded because
# it depends on STM32Cube HAL headers that are not part of this repository.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
build="${root}/build-analysis"

if ! command -v clang-tidy >/dev/null 2>&1; then
  echo "clang-tidy is not installed" >&2
  exit 1
fi
if ! command -v cppcheck >/dev/null 2>&1; then
  echo "cppcheck is not installed" >&2
  exit 1
fi

cmake -S "${root}/firmware" -B "${build}" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build "${build}" --parallel

mapfile -t sources < <(find "${root}/firmware/src" -name '*.c' -print | sort)
clang-tidy -p "${build}" --warnings-as-errors='*' "${sources[@]}"

cppcheck \
  --enable=warning,style,performance,portability \
  --std=c99 \
  --error-exitcode=1 \
  --inline-suppr \
  --suppress=missingIncludeSystem \
  --suppress=unusedFunction \
  --suppress=unmatchedSuppression \
  -I "${root}/firmware/include" \
  "${root}/firmware/src"
