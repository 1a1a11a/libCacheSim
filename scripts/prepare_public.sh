#!/bin/bash 

# prepare the repo for public release

SORUCE=$(readlink -f ${BASH_SOURCE[0]})
DIR=$(dirname ${SORUCE})

rm -r ${DIR}/customized;
rm -r ${DIR}/priv;


cd ${DIR}/../libCacheSim/; 
# find . -name priv -type d;
find . -name priv -type d -exec rm -rf {} \; -prune

git remote remove origin;
git remote add origin git@github.com:1a1a11a/libCacheSim.git;

CURR_BRANCH=$(git rev-parse --abbrev-ref HEAD);
NEW_BRANCH="staging_${RANDOM}";
git checkout -b ${NEW_BRANCH};
git push -u origin --set-upstream ${NEW_BRANCH};

