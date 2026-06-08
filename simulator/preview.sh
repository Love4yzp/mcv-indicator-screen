#!/usr/bin/env bash
# Preview script for SenseCAP Indicator LVGL simulator.
#
# Usage:
#   ./simulator/preview.sh                  # Build & launch interactive mode
#   ./simulator/preview.sh --screenshot     # Build & capture all pages to simulator/screenshots/
#   ./simulator/preview.sh --no-build       # Skip build, just launch
#
# Prerequisites: SDL2 (brew install sdl2), CMake

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
SIM_BIN="$BUILD_DIR/sim"
SCREENSHOT_DIR="$SCRIPT_DIR/screenshots"

DO_BUILD=true
MODE=interactive

for arg in "$@"; do
    case "$arg" in
        --screenshot)  MODE=screenshot ;;
        --inspect)     MODE=inspect ;;
        --no-build)    DO_BUILD=false ;;
        --help|-h)
            echo "Usage: $0 [--screenshot|--inspect] [--no-build]"
            echo "  (default)      Build and launch interactive simulator"
            echo "  --screenshot   Capture PNG for every page → simulator/screenshots/"
            echo "  --inspect      Capture PNG + JSON widget tree for every page"
            echo "  --no-build     Skip build step"
            exit 0
            ;;
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

if $DO_BUILD; then
    echo "==> Configuring..."
    cmake -B "$BUILD_DIR" -S "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE=Release -Wno-dev 2>&1 | tail -1
    echo "==> Building..."
    cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" 2>&1 | grep -E '^\[100%\]|error:' || true
    [[ -x "$SIM_BIN" ]] || { echo "ERROR: Build failed"; exit 1; }
    echo "==> Build OK"
fi

case "$MODE" in
    screenshot)
        mkdir -p "$SCREENSHOT_DIR"
        "$SIM_BIN" --screenshot-all "$SCREENSHOT_DIR"
        echo "==> Screenshots saved to $SCREENSHOT_DIR/"
        ;;
    inspect)
        mkdir -p "$SCREENSHOT_DIR"
        "$SIM_BIN" --inspect-all "$SCREENSHOT_DIR"
        echo "==> PNG + JSON widget trees saved to $SCREENSHOT_DIR/"
        ;;
    interactive)
        echo "==> Launching simulator (close window to exit)..."
        exec "$SIM_BIN"
        ;;
esac
