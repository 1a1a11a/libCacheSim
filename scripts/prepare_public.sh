#!/bin/bash 

# prepare the repo for public release

SORUCE=$(readlink -f ${BASH_SOURCE[0]})
DIR=$(dirname ${SORUCE})

rm -r ${DIR}/priv;
rm -r ${DIR}/customized;

cd ${DIR}/../libCacheSim/; 
find . -name priv -type d -exec rm -rf {} \;



