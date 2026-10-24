#!/bin/sh
# Build tvterm (dynamic and/or static executable).
#
# Usage:  scripts/build.sh [dynamic|static|both]     (default: both)
# Output: bin/tvterm-dynamic, bin/tvterm-static, bin/build-*.log
#
# Required packages:
#   Debian/Ubuntu/Raspbian: apt install build-essential cmake perl patch git \
#                                       libncurses-dev libgpm-dev
#   Alpine:                 apk add build-base cmake perl patch git linux-headers \
#                                   ncurses-dev ncurses-static gpm-dev
#   (on Alpine, drop gpm-dev if it is not available; gpm is optional)
set -e

mode=${1:-both}
root=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$root/bin"
stamp=$(date '+%Y%m%d-%H%M%S')

build() # $1 = name, $2.. = extra cmake options
{
    name=$1; shift
    dir="$root/build-$name"
    log="$root/bin/build-$name-$stamp.log"
    echo ">> Building $name (log: $log)"
    rm -rf "$dir"
    cmake -S "$root" -B "$dir" -DCMAKE_BUILD_TYPE=MinSizeRel "$@" >"$log" 2>&1
    cmake --build "$dir" -j "$(nproc 2>/dev/null || echo 2)" >>"$log" 2>&1
    cp "$dir/tvterm" "$root/bin/tvterm-$name"
    strip "$root/bin/tvterm-$name" 2>/dev/null || true
    ls -la "$root/bin/tvterm-$name"
}

case $mode in
    dynamic) build dynamic ;;
    static)  build static -DTVTERM_STATIC=ON ;;
    both)    build dynamic; build static -DTVTERM_STATIC=ON ;;
    *)       echo "Usage: $0 [dynamic|static|both]" >&2; exit 1 ;;
esac
