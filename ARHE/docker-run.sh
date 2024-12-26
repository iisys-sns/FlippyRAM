#!/bin/bash

docker build -t arhe .
mkdir -p data
docker run -it --privileged -v /lib/modules:/lib/modules -v $(pwd)/data:/arhe/ARHE/data arhe
