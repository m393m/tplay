# TPlay Turbo

**Ultra-lightweight headless terminal graphics compositor, RDP client & TUI launcher for Linux and WSL2.**

TPlay Turbo renders full X11, OpenGL, Vulkan applications, web browsers, RDP sessions, and 3D games directly inside a raw Linux TTY or terminal emulator — with zero desktop environment (KDE/GNOME/Wayland) required on the host or client.

---

## Key Features

* **Unified TUI Launcher:** Automatically discovers native `.desktop` apps, **Flatpak**, **Snap**, and **AppImage** binaries with instant fuzzy search.
* **Frameless Web Apps (`URL` mode):** Launches websites in isolated Chromium/Brave/Firefox app windows with clean per-session profiles.
* **Built-in RDP Client (`v2.2.0`):** Connect directly to Windows/Linux RDP servers from the TUI menu (`xfreerdp3` / `xfreerdp` / `rdesktop`) with `/dynamic-resolution` support — works great over SSH jump hosts and inside **WSL2 + Windows Terminal**.
* **Dynamic Viewport Scaling (`F12` / `Ctrl+Y`):** Switch live between fixed **Stretch (`1280x720`)** and **Dynamic XRANDR**, which adapts the virtual `Xvfb` framebuffer resolution and mouse coordinates to match your terminal window aspect ratio in real time.
* **SSH Clipboard Bridge (`OSC 52`):** Text copied (`Ctrl+C`) inside the virtual X11 session automatically syncs to your local PC or mobile SSH client clipboard (Kitty, Alacritty, WezTerm, Foot, Windows Terminal, Termius, Termux).
* **Multithreaded C Zone-Diff Renderer:** Splits the frame into `16x8` zones, rendering only dirty regions via ANSI TrueColor half-blocks (`▀`) with Synchronized Output (`\x1b[?2026h`) to eliminate tearing and minimize SSH bandwidth.
* **Hardware VA-API Downscaling:** Uses `/dev/dri/renderD128` for GPU-accelerated frame scaling when available, falling back smoothly to fast bilinear CPU scaling.
* **Automatic Font & HiDPI Management:** Temporarily switches supported terminals (Ptyxis, Kitty, XTerm-compatible) to a crisp `4pt` pixel font (`TPlayPixel.ttf`), enforces `1:1 (96 DPI)` scaling inside `Xvfb`, and cleanly restores your terminal upon exit.

---

## Hotkeys (Inside Session)

| Shortcut | Action |
| :--- | :--- |
| **`F12`** or **`Ctrl+Y`** | Toggle **Stretch (`1280x720`)** ↔ **Dynamic XRANDR** viewport mode |
| **`Ctrl+L`** | Force fit/maximize active windows and redraw screen |
| **`Ctrl+\`** | Exit session and restore terminal settings |

---

## Installation

### 1. Fedora / RHEL (`.rpm`)
Download `tplay-turbo-2.2.0-1.fc45.x86_64.rpm` from **[Releases](https://github.com)** and install via `dnf`:
```bash
sudo dnf install -y ./tplay-turbo-2.2.0-1.*.x86_64.rpm
```

### 2. Ubuntu / Debian / WSL2 (`.deb`)
Download `tplay-turbo_2.2.0_amd64.deb` from **[Releases](https://github.com)** and install via `apt`:
```bash
sudo apt update
sudo apt install -y ./tplay-turbo_2.2.0_amd64.deb
```

### 3. Universal Linux Tarball (`.tar.gz` — Arch, Gentoo, etc.)
Download `tplay-turbo-2.2.0-linux-x86_64.tar.gz` from **[Releases](https://github.com)**, extract, and run the installer (ensure `python3`, `ffmpeg`, `xvfb`, `xdotool`, `xrandr`, `xclip`, and `fontconfig` are installed):
```bash
tar -xzf tplay-turbo-2.2.0-linux-x86_64.tar.gz
cd tplay-turbo-2.2.0
./install.sh
```

### 4. Build from Source
```bash
git clone https://github.com
cd tplay
make
sudo make install
sudo fc-cache -f /usr/share/fonts/tplay
```

---

## Usage

Launch the interactive TUI menu:
```bash
tplay-turbo
```

Or launch any command directly:
```bash
tplay-turbo firefox
tplay-turbo blender
tplay-turbo xfreerdp /v:192.168.1.50 /u:Administrator
```

Limit target frame rate via environment variable (optional):
```bash
TPLAY_FPS=24 tplay-turbo
```

---

## Architecture

1. **Virtual Display (`Xvfb`):** Starts an isolated headless X11 server (`96 DPI`, `GLX` + `RANDR` enabled) with `1:1` UI scaling.
2. **Capture & Scale (`ffmpeg`):** Grabs the virtual framebuffer and scales it directly to your terminal's `cols x (rows * 2)` pixel grid via VA-API or `fast_bilinear`.
3. **Diff Engine (`libtplay_turbo.so`):** Multithreaded C library comparing `16x8` blocks against the previous frame and emitting minimal TrueColor ANSI sequences.
4. **Input & Clipboard Bridge:** Translates SGR mouse events (`1006`) and keyboard input via `xdotool`, while syncing X11 clipboard changes to your local terminal via `OSC 52`.

---

## License

MIT License
