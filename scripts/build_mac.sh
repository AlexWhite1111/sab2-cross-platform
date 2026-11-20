#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build-mac -G Ninja -DCMAKE_BUILD_TYPE=Release -DSAB2_WITH_OPENCV=ON
cmake --build build-mac --parallel
ctest --test-dir build-mac --output-on-failure
printf '\nStart: ./build-mac/sab2_demo --demo --frames 10\n'
