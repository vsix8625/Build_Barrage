#!/usr/bin/env bash
set -euo pipefail

if [ "$EUID" -eq 0 ]; then
  echo "[build.sh]: Error: Do not run the bootstrap as root."
  exit 1
fi

START_TIME=$(date +%s)

BUILD_DIR="bootstrap"
mkdir -p "$BUILD_DIR"

CC=""
LD=""
LDFLAGS=()

if command -v clang >/dev/null 2>&1
then
    if command -v mold >/dev/null 2>&1
    then
        CC="clang"
		LDFLAGS=("-fuse-ld=mold")
        echo "[build.sh]: Using clang + mold (recommended)..."
		cp examples/Recommended Barrfile
    elif command -v ld.lld >/dev/null 2>&1
    then
        CC="clang"
		LDFLAGS=("-fuse-ld=lld")
        echo "[build.sh]: Using clang + lld..."
		cp examples/Clang Barrfile
    else
        CC="clang"
        echo "[build.sh]: Using clang + system ld..."
		cp examples/Clang Barrfile
    fi
elif command -v gcc >/dev/null 2>&1
then
    CC="gcc"
    echo "[build.sh]: Using gcc..."
	cp examples/Bootstrap Barrfile
elif command -v cc >/dev/null 2>&1
then
    CC="cc"
    echo "[build.sh]: Using cc..."
	cp examples/Bootstrap Barrfile
else
    echo "[build.sh]: Error: No C compiler found."
    exit 1
fi

ARGS=(
    -std=c2x
    -O2
    -I. 
    -Isrc 
    -Isrc/barr_build_system 
    -Isrc/barr_commands 
    -Isrc/barr_platform/linux 
    -Isrc/olmos
    -D_GNU_SOURCE 
    -DNDEBUG 
    -DBARR_BOOTSTRAP
    -o "$BUILD_DIR/barr"
)

#SOURCES=$(find src -name "*.c")
mapfile -t SOURCES < <(find src -name "*.c")

$CC "${ARGS[@]}" "${LDFLAGS[@]}" "${SOURCES[@]}" -lxxhash -lpthread  &
BARR_PID=$!

width=40
printf "Building: ["
if command -v ps >/dev/null 2>&1; then
while ps -p $BARR_PID > /dev/null; do
    printf "-"
    sleep 0.2
done
fi

if wait $BARR_PID; then
    printf "\r\033[KBuilding: [----------------------------------------] 100%%\n"
else
    printf "\r\033[K[build.sh]: Error: Compilation failed.\n"
    exit 1
fi


if [ -f "./$BUILD_DIR/barr" ]; then
    echo "[build.sh]: Rebuilding with barr..."
    echo 

	 BARR_BIN="$(pwd)/$BUILD_DIR/barr"

	(
		cd tools/fo || exit 1
		$BARR_BIN -I -s
	)

    $BARR_BIN -I -s
    $BARR_BIN -rb
    
    echo 
    echo 
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
