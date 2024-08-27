#!/usr/bin/env bash

set -e -o pipefail

rm -rf deps
mkdir -p deps

# Setup AIGER
rm -rf aiger-1.9.18.tar.gz
wget https://github.com/arminbiere/aiger/archive/refs/tags/rel-1.9.18.tar.gz
tar xf rel-1.9.18.tar.gz
mv aiger-rel-1.9.18 deps/aiger


# Setup Boolector
git clone https://github.com/boolector/boolector deps/boolector

cd deps/boolector
git checkout bitblast-api

./contrib/setup-btor2tools.sh
./contrib/setup-lingeling.sh
./configure.sh --prefix $(pwd)/../install
cd build

make -j$(nproc) install
