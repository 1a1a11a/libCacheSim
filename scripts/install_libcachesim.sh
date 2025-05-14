#!/bin/bash 
set -euo pipefail

SOURCE=$(readlink -f ${BASH_SOURCE[0]})
DIR=$(dirname $SOURCE})

cd ${DIR}/../;
mkdir _build || true 2>/dev/null;
cd _build;



cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="-Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-unused-but-set-variable -Wpedantic -Wformat=2 -Wformat-security -Wshadow -Wwrite-strings -Wstrict-prototypes -Wold-style-definition -Wredundant-decls -Wnested-externs -Wmissing-include-dirs" \
      -DCMAKE_CXX_FLAGS="-Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-unused-but-set-variable -Wpedantic -Wformat=2 -Wformat-security -Wshadow -Wwrite-strings -Wstrict-prototypes -Wold-style-definition -Wredundant-decls -Wnested-externs -Wmissing-include-dirs" \
      ..

make -j;
cd ${DIR}; 

