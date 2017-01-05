#!/bin/bash
#-------------------------------------------------------------------------------------------------------
# Copyright (C) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#-------------------------------------------------------------------------------------------------------

LLVM_VERSION="3.9.1"

echo -e "This will take some time... [and free memory 4GB+]\n"

ROOT=${PWD}/cc-toolchain/
if [ ! -d ./cc-toolchain/src/llvm/projects/compiler-rt ]; then
    rm -rf cc-toolchain
    mkdir cc-toolchain
    cd cc-toolchain
    mkdir src
    cd src
    echo "Downloading LLVM ${LLVM_VERSION}"
    wget –quiet "http://llvm.org/releases/${LLVM_VERSION}/llvm-${LLVM_VERSION}.src.tar.xz" >/dev/null 2>&1
    tar -xf "llvm-${LLVM_VERSION}.src.tar.xz"
    if [ $? == 0 ]; then
        rm "llvm-${LLVM_VERSION}.src.tar.xz"
        mv "llvm-${LLVM_VERSION}.src" llvm
    else
        exit 1
    fi

    cd llvm/tools/
    echo "Downloading Clang ${LLVM_VERSION}"
    wget –quiet "http://llvm.org/releases/${LLVM_VERSION}/cfe-${LLVM_VERSION}.src.tar.xz" >/dev/null 2>&1
    tar -xf "cfe-${LLVM_VERSION}.src.tar.xz"
    if [ $? == 0 ]; then
        mv "cfe-${LLVM_VERSION}.src" clang
        rm "cfe-${LLVM_VERSION}.src.tar.xz"
    else
        exit 1
    fi

    mkdir -p ../projects/
    cd ../projects/
    echo "Downloading Compiler-RT ${LLVM_VERSION}"
    wget –quiet "http://llvm.org/releases/${LLVM_VERSION}/compiler-rt-${LLVM_VERSION}.src.tar.xz" >/dev/null 2>&1
    tar -xf "compiler-rt-${LLVM_VERSION}.src.tar.xz"
    if [ $? == 0 ]; then
        mv "compiler-rt-${LLVM_VERSION}.src" compiler-rt
        rm "compiler-rt-${LLVM_VERSION}.src.tar.xz"
    else
        exit 1
    fi
fi

mkdir -p "${ROOT}/build"
cd "${ROOT}/src/llvm"
mkdir -p build_
cd build_

cmake ../ -DCMAKE_INSTALL_PREFIX="${ROOT}/build"  >/dev/null 2>&1

if [ $? != 0 ]; then
    cd ..
    rm -rf build_
    mkdir build_
    cd build_
    cmake ../ -DCMAKE_INSTALL_PREFIX="${ROOT}/build"
fi

if [ $? != 0 ]; then
    exit 1
fi

# do not use -j option here as it consumes more than 4GB memory while linking clang
make install

if [ $? == 0 ]; then
    echo -e "Done!\n./build.sh args are given below;\n\n"
    echo "--cxx=${ROOT}/build/bin/clang++ --cc=${ROOT}/build/bin/clang"
fi
