#!/bin/bash -eu

mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER="${CC}" \
      -DCMAKE_CXX_COMPILER="${CXX}" \
      -DCMAKE_C_FLAGS="${CFLAGS}" \
      -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
      ..
make -j$(nproc)

# Copy the fuzzer binaries to the output directory expected by ClusterFuzzLite
cp fuzz_query $OUT/
cp fuzz_transaction $OUT/
