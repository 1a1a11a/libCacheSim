#!/bin/bash 
set -euo pipefail


SORUCE=$(readlink -f ${BASH_SOURCE[0]})
DIR=$(dirname ${the_source})

cd ${DIR}/../;
mkdir _build;
cd _build;
cmake ..;
make -j;
cd ${DIR}; 

