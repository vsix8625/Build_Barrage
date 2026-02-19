#!/usr/bin/env bash
set -euo pipefail

CMAKE_BUILD_DIR="bootstrap"

mkdir -p $CMAKE_BUILD_DIR 

echo [build.sh]: Configurating cmake... 
echo 

if command -v ninja &> /dev/null; then
	GEN="-G Ninja"
    echo "[build.sh]: Ninja found, using Ninja generator"
else
	GEN=""
    echo "[build.sh]: Ninja not found, using make"
fi

echo 
echo [build.sh]: Building with cmake...
echo 

cmake -S . -B $CMAKE_BUILD_DIR $GEN
cmake --build $CMAKE_BUILD_DIR

echo 
echo [build.sh]: Rebuilding with barr...
echo 

./$CMAKE_BUILD_DIR/bin/Release/barr-cmake -c -nc
./$CMAKE_BUILD_DIR/bin/Release/barr-cmake build

echo [build.sh]: Done

rm -rf $CMAKE_BUILD_DIR
