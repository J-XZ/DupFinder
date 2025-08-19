#!/usr/bin/bash

mkdir -p ./tmp
rm -f ./tmp/output.json
./build.sh
time ./build/find_dup_files /root/code/FUSE_test ./tmp/output.json
