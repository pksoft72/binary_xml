#!/bin/bash
make || exit 1
cd build/*
#./test-creating &&
#./xb2xml -v test3.xb
#./xb2xml test4.xb
watch ./xb2xml /dev/shm/1/archive-current.xbw
