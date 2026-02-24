#!/usr/bin/env bash
set -euo pipefail

START_TIME=$(date +%s)

BUILD_DIR="bootstrap"
mkdir -p "$BUILD_DIR"

COMPILER=""
for cmd in clang gcc cc; do
    if command -v "$cmd" >/dev/null 2>&1; then
        COMPILER="$cmd"
        break
    fi
done

if [ -z "$COMPILER" ]; then
    echo "[build.sh]: Error: No C compiler found."
    exit 1
fi

L_STRAT=""
if command -v mold >/dev/null 2>&1; then
    L_STRAT="-fuse-ld=mold"
elif command -v ld.lld >/dev/null 2>&1; then
    L_STRAT="-fuse-ld=lld"
fi

ARGS=(
    -std=c23
    -O2
    -fPIE
    -I. 
    -Isrc 
    -Isrc/barr_build_system 
    -Isrc/barr_commands 
    -Isrc/barr_platform/linux 
    -Isrc/olmos
    -D_GNU_SOURCE 
    -DNDEBUG 
    -mavx
    -o "$BUILD_DIR/barr"
    -lxxhash 
    -lpthread
)

SOURCES=$(find src -name "*.c")

echo "[build.sh]: Using $COMPILER..."

$COMPILER "${ARGS[@]}" $SOURCES $L_STRAT &
BARR_PID=$!

width=40
printf "Building: ["
while ps -p $BARR_PID > /dev/null; do
    printf "━"
    sleep 0.2
done

printf "\r\033[KBuilding: [━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━] 100%%\n"

wait $BARR_PID

if [ -f "./$BUILD_DIR/barr" ]; then
    echo "[build.sh]: Rebuilding with barr..."
    echo 
    rm -rf .barr
    ./$BUILD_DIR/barr -I
    ./$BUILD_DIR/barr -rb
    
    echo "[build.sh]: Done, cleaning up..."
    rm -rf "$BUILD_DIR"
else
    echo "[build.sh]: Error: Bootstrap failed."
    exit 1
fi


END_TIME=$(date +%s)
TOTAL_TIME=$((END_TIME - START_TIME))

echo "--------------------------------------"
echo "[build.sh]: Total process took ${TOTAL_TIME}s"
