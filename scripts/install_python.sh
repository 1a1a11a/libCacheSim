rm -rf ./build
cmake -G Ninja -B build
pushd libCacheSim-python
pip install -e . -vvv
popd
python -c "import libCacheSim"
pushd libCacheSim-python
pytest .
popd