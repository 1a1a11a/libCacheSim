#!/bin/bash
set -euo pipefail

SORUCE=$(readlink -f "${BASH_SOURCE[0]}")
DIR=$(dirname "${SORUCE}")

cd "${DIR}"/../
mkdir -p _build
cd _build
cmake ..
make -j
cd "${DIR}"
