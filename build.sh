#! /bin/sh

set -e

export VERSION=0.0.1
export BUILD_TYPE="Debug"

BUILD_DIR=build/$BUILD_TYPE

cmake --preset linux
cmake --build $BUILD_DIR
cmake --install $BUILD_DIR

mv $BUILD_DIR/compile_commands.json ./
