#!/usr/bin/env bash
set -e

REPO_URL="https://github.com/m393m/tplay.git"
TMP_DIR=$(mktemp -d /tmp/tplay-build-XXXXXX)
cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT

echo "=== TPlay Turbo 2.2.1 — One-Line Source Builder & Installer ==="

if command -v dnf >/dev/null 2>&1; then
    echo "[1/4] Detected Fedora/RHEL — installing dependencies via dnf..."
    sudo dnf install -y git gcc make python3 ffmpeg-free xorg-x11-server-Xvfb xdotool xrandr xclip fontconfig || true
elif command -v apt-get >/dev/null 2>&1; then
    echo "[1/4] Detected Debian/Ubuntu/WSL2 — installing dependencies via apt..."
    sudo apt-get update
    sudo apt-get install -y git build-essential python3 ffmpeg xvfb xdotool x11-xserver-utils xclip fontconfig || true
elif command -v pacman >/dev/null 2>&1; then
    echo "[1/4] Detected Arch Linux — installing dependencies via pacman..."
    sudo pacman -Sy --noconfirm git base-devel python ffmpeg xorg-server-xvfb xdotool xorg-xrandr xclip fontconfig || true
else
    echo "[1/4] Unknown package manager — make sure git, gcc, make, python3, ffmpeg, Xvfb, xdotool, xrandr, xclip, fontconfig are installed."
fi

if [ -f "./libtplay_turbo.c" ] && [ -f "./Makefile" ] && [ -f "./tplay-turbo" ]; then
    echo "[2/4] Using local source files..."
    cp -r ./* "$TMP_DIR/"
else
    echo "[2/4] Cloning repository from GitHub..."
    git clone --depth 1 "$REPO_URL" "$TMP_DIR"
fi

echo "[3/4] Compiling libtplay_turbo.so..."
cd "$TMP_DIR"
make

echo "[4/4] Installing TPlay Turbo into system..."
sudo make install
sudo fc-cache -f /usr/share/fonts/tplay >/dev/null 2>&1 || true

echo ""
echo "✅ TPlay Turbo 2.2.1 successfully built and installed!"
echo "👉 Run it with: tplay-turbo"   