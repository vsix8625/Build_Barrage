#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="bootstrap"

mkdir -p $BUILD_DIR # tmp dir for bootstrap

for cmd in clang gcc cc; do
    if command -v "$cmd" >/dev/null 2>&1; then
        COMPILER="$cmd"
        break
    fi
done

if ! $COMPILER -std=c23 -dM -E - < /dev/null > /dev/null 2>&1; then
    echo "------------------------------------------------------------"
    echo "ERROR: Prehistoric compiler: ($COMPILER) detected."
    echo "Aborting!!"
    echo "------------------------------------------------------------"
    exit 1
fi

L_STRAT=""
if command -v mold >/dev/null 2>&1; then
    L_STRAT="-fuse-ld=mold"
elif command -v ld.lld >/dev/null 2>&1; then
    L_STRAT="-fuse-ld=lld"
fi


echo [build.sh]: Using $COMPILER...
$COMPILER -std=c23 -O2 -fPIE \
    -I. -Isrc -Isrc/barr_build_system -Isrc/barr_commands -Isrc/barr_platform/linux -Isrc/olmos \
    $(find src -name "*.c") \
    -D_GNU_SOURCE -DNDEBUG -mavx \ 
    -o bootstrap/barr -lxxhash -lpthread $L_STRAT

echo [build.sh]: Rebuilding with barr...
echo 

rm -rf .barr             # removing any previous barr stale state 
./$BUILD_DIR/barr -I     # initializing the cmake created barr
./$BUILD_DIR/barr -rb    # rebuild barr 

echo [build.sh]: Done, cleaning up...
rm -rf $BUILD_DIR 


