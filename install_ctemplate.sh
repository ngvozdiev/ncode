#!/bin/sh
set -ex
git clone https://github.com/OlafvdSpek/ctemplate.git
cd ctemplate && ./autogen.sh && ./configure --prefix=/usr && make && sudo make install
