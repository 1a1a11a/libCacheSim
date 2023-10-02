#!/bin/bash 
set -euxo pipefail

git clone git@github.com:1a1a11a/libCacheSim.git
cd libCacheSim
git remote add priv git@github.com:1a1a11a/libCacheSimPrv.git
git fetch priv
git checkout -b mergePriv
git merge priv/develop
