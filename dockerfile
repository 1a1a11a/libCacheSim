
FROM ubuntu:22.04 as base
MAINTAINER Juncheng
LABEL version="0.0.2"


WORKDIR /

# install dependency
# RUN apt update && apt-get install -yqq libglib2.0-dev libgoogle-perftools-dev cmake git sudo wget gcc g++ xxhash
RUN apt update && apt-get install -yqq cmake git sudo wget

# clone repo
RUN git clone https://github.com/1a1a11a/libCacheSim -b develop

# build libCacheSim
WORKDIR /libCacheSim/
# RUN mkdir _build;
RUN bash ./scripts/install_dependency.sh
RUN bash ./scripts/install_libcachesim.sh


WORKDIR /libCacheSim/_build/
# RUN cmake -DSUPPORT_ZSTD_TRACE=on .. && make -j && sudo make install


WORKDIR /libCacheSim/_build/bin/
# CMD ls bin/


# build the docker file
# sudo docker build -t 1a1a11a/libcachesim -f dockerfile .

# push to docker hub
# sudo docker tag 1a1a11a/libcachesim:latest 1a1a11a/libcachesim:0.0.2
# sudo docker push 1a1a11a/libcachesim:latest
# sudo docker push 1a1a11a/libcachesim:0.0.2

# use the container
# sudo docker run -v /local/data/path:/data -it 1a1a11a/libcachesim:latest bash

# docker install instructions can be found at https://docs.docker.com/engine/install/
# for ubuntu: 
# 
# sudo apt-get update && sudo apt-get install -yqq ca-certificates curl gnupg; 
# sudo install -m 0755 -d /etc/apt/keyrings && \
# curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg && \
# sudo chmod a+r /etc/apt/keyrings/docker.gpg
# echo \
#   "deb [arch="$(dpkg --print-architecture)" signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
#   "$(. /etc/os-release && echo "$VERSION_CODENAME")" stable" | \
#   sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
# sudo apt update && sudo apt-get install -yqq docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin


