#!/usr/bin/env bash

set -e -o pipefail

rm -rf deps/aiger aiger-1.9.4.tar.gz
wget http://fmv.jku.at/aiger/aiger-1.9.4.tar.gz
tar xf aiger-1.9.4.tar.gz

mkdir -p deps
mv aiger-1.9.4 deps/aiger
