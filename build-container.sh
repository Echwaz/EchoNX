#!/usr/bin/env bash
set -euo pipefail
cd /work
source /opt/devkitpro/switchvars.sh
export PATH="/opt/devkitpro/devkitA64/bin:/opt/devkitpro/tools/bin:$PATH"
dkp-pacman -S --noconfirm --needed switch-sdl2_ttf switch-sdl2_image
if ! aarch64-none-elf-pkg-config --exists libavformat libavcodec libswresample libcurl; then
    echo "Missing Switch FFmpeg/curl portlibs. Use a portlibs volume containing the Switch builds."
    exit 1
fi
cmake -S /work -B /work/build-switch -DCMAKE_BUILD_TYPE=Release -DPKG_CONFIG_EXECUTABLE="$(command -v aarch64-none-elf-pkg-config)"
cmake --build /work/build-switch -j4 --target echonx.nro
