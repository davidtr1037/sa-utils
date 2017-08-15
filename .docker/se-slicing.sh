#!/bin/bash -x
# Make sure we exit if there is a failure
set -e

make \
    LLVM_SRC=${BUILD_DIR}/llvm-3.4.2.src \
    LLVM_OBJ=${BUILD_DIR}/llvm-3.4.2-build \
    SVF_PATH=${SRC_DIR}/SVF \
    DG_PATH=${SRC_DIR}/dg \
    all
