#!/usr/bin/env bash

mkdir -p build && pushd build

cmake -GNinja -DCMAKE_CXX_FLAGS="$CXXFLAGS" -DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS" -DCMAKE_VERBOSE_MAKEFILE=ON ..

ninja

popd || true
