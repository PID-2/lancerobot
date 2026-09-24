#!/bin/bash
# Installs the ESP32 toolchain the sketch is built with, then compiles the sketch both
# ways as a smoke test. Idempotent: re-run it to repair or re-verify an install.
#
#   esp32/tools/install-arduino.sh               install, then compile both configurations
#   esp32/tools/install-arduino.sh --no-compile  install only
#
# Everything lives under /opt/arduino (binary, config, core, libraries). A wrapper at
# /usr/local/bin/arduino-cli carries the config path, so plain `arduino-cli` works from
# any shell. Versions are pinned to what the sketch was verified against (HANDOFF.md).
# Written for the Claude Code container (Linux x86-64, root): about 3 minutes to install,
# about 3 more for the two compiles.
#
# Network notes for the container: GitHub release downloads, espressif.github.io and
# downloads.arduino.cc are reachable; the GitHub releases web page and api.github.com
# are not, which is why versions are pinned here instead of discovered. HTTPS_PROXY is
# passed to arduino-cli when set (the container sets it).
set -euo pipefail

CLI_VERSION=1.5.1
CORE_VERSION=3.3.12
VL53L0X_TAG=1.3.1        # Pololu VL53L0X
MPU6050_TAG=v1.4.5       # Electronic Cats MPU6050 (I2Cdev + MPU6050 by Jeff Rowberg)
ESP32_INDEX=https://espressif.github.io/arduino-esp32/package_esp32_index.json

ROOT=/opt/arduino
CFG=$ROOT/data/arduino-cli.yaml
REPO=$(cd "$(dirname "$0")/../.." && pwd)
SKETCH=$REPO/esp32/micromouse_esp32
FQBN=esp32:esp32:esp32c6

cli()  { "$ROOT/bin/arduino-cli" --config-file "$CFG" "$@"; }
step() { echo; echo "== $(date -u +%H:%M:%S) $*"; }

mkdir -p "$ROOT/bin" "$ROOT/data" "$ROOT/downloads" "$ROOT/user/libraries"

step "arduino-cli $CLI_VERSION"
if ! "$ROOT/bin/arduino-cli" version 2>/dev/null | grep -q "Version: $CLI_VERSION "; then
  curl -fsSL -o "$ROOT/arduino-cli.tar.gz" \
    "https://github.com/arduino/arduino-cli/releases/download/v${CLI_VERSION}/arduino-cli_${CLI_VERSION}_Linux_64bit.tar.gz"
  tar -xzf "$ROOT/arduino-cli.tar.gz" -C "$ROOT/bin" arduino-cli
  rm -f "$ROOT/arduino-cli.tar.gz"
fi
"$ROOT/bin/arduino-cli" version

# The wrapper must be a real file. Remove first: writing through a symlink here would
# overwrite the binary it points at.
rm -f /usr/local/bin/arduino-cli
printf '#!/bin/sh\nexec %s/bin/arduino-cli --config-file %s "$@"\n' "$ROOT" "$CFG" > /usr/local/bin/arduino-cli
chmod +x /usr/local/bin/arduino-cli

step "config: $CFG"
"$ROOT/bin/arduino-cli" config init --dest-file "$CFG" --overwrite > /dev/null
cli config set directories.data "$ROOT/data"
cli config set directories.downloads "$ROOT/downloads"
cli config set directories.user "$ROOT/user"
cli config set board_manager.additional_urls "$ESP32_INDEX"
if [ -n "${HTTPS_PROXY:-}" ]; then cli config set network.proxy "$HTTPS_PROXY"; fi

step "core esp32:esp32@$CORE_VERSION"
cli core update-index || echo "NOTE: an index failed to download; continuing, only the esp32 index is needed"
cli core install "esp32:esp32@$CORE_VERSION"

step "libraries at pinned tags (arduino-cli lib install would take the latest)"
if [ ! -d "$ROOT/user/libraries/VL53L0X" ]; then
  git -c advice.detachedHead=false clone -q --depth 1 --branch "$VL53L0X_TAG" \
    https://github.com/pololu/vl53l0x-arduino "$ROOT/user/libraries/VL53L0X"
fi
if [ ! -d "$ROOT/user/libraries/MPU6050" ]; then
  git -c advice.detachedHead=false clone -q --depth 1 --branch "$MPU6050_TAG" \
    https://github.com/ElectronicCats/mpu6050 "$ROOT/user/libraries/MPU6050"
fi

step "installed"
cli core list
cli lib list

if [ "${1:-}" = "--no-compile" ]; then echo; echo "INSTALL DONE (compile skipped)"; exit 0; fi

compile() {   # compile <label> [extra arduino-cli args...]
  local label=$1; shift
  step "compile $label"
  local log; log=$(mktemp)
  if cli compile --fqbn "$FQBN" --warnings all "$@" "$SKETCH" > "$log" 2>&1; then
    grep -E 'warning:|Sketch uses|Global variables' "$log" || true
    rm -f "$log"
  else
    tail -30 "$log"; rm -f "$log"
    echo "FAILED: compile $label"; exit 1
  fi
}
compile "HARDWARE_READY=0 (dry run, as shipped)"
compile "HARDWARE_READY=1 (real hardware layer)" --build-property "compiler.cpp.extra_flags=-DHARDWARE_READY=1"

echo; echo "INSTALL DONE"
