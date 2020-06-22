#!/bin/bash


inc=$(pkg-config --cflags libCacheSim)
lib=$(pkg-config --libs libCacheSim)
if [[ "${inc}" == "" ]]; then echo "pkg-config --cflags libCacheSim empty $(pkg-config --cflags libCacheSim)"; exit 1; fi
if [[ "${lib}" == "" ]]; then echo "pkg-config --libs libCacheSim empty $(pkg-config --libs libCacheSim)"; exit 1; fi
echo "lib test has passed"

