#!/bin/sh
set -ex
git clone https://github.com/google/protobuf.git
cd protobuf && ./autogen.sh && ./configure --prefix=/usr && make && sudo make install
