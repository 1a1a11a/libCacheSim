#!/bin/bash
set -euo pipefail

# Build the main libCacheSim C++ library first
echo "Building main libCacheSim library..."
rm -rf ./build
cmake -G Ninja -B build # -DENABLE_3L_CACHE=ON
ninja -C build

# Now build and install the Python binding
echo "Building Python binding..."
echo "Sync python version..."
python scripts/sync_python_version.py
pushd libCacheSim-python
pip install -e . -vvv
popd

# Test that the import works
echo "Testing import..."
python -c "import libcachesim"

# Run tests
echo "Running tests..."
pushd libCacheSim-python
pytest .
popd
