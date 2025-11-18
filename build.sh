#! /bin/sh

set -e

export VERSION=0.0.1
export BUILD_TYPE="Debug"

cmake --preset linux
cmake --build build

mv build/compile_commands.json ./
