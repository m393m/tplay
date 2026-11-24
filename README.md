# TPlay Turbo

**Ultra-lightweight headless terminal graphics compositor, RDP client & TUI launcher for Linux and WSL2.**

TPlay Turbo renders full X11, OpenGL, Vulkan applications, web browsers, RDP sessions, and 3D games directly inside a raw Linux TTY or terminal emulator — with zero desktop environment (KDE/GNOME/Wayland) required on the host or client.

---

## Quick One-Line Install (Build from Source)

Install dependencies, compile the C renderer, and install `tplay-turbo` automatically on **Fedora, Ubuntu/Debian/WSL2, or Arch Linux**:

```bash
curl -fsSL https://raw.githubusercontent.com/m393m/tplay/main/install.sh | bash
```

---

## Key Features

* **Unified TUI Launcher:** Automatically discovers native `.desktop` apps, **Flatpak**, **Snap**, and **AppImage** binaries with instant fuzzy search.
* **Frameless Web Apps (`URL` mode):** Launches websites in isolated Chromium/Brave/Firefox app windows with clean per-session profiles.
* **Built-in RDP Client (`v2.2.0+`):** Connect directly to Windows/Linux RDP servers from the TUI menu (`xfreerdp3` / `xfreerdp` / `rdesktop`) with `/dynamic-resolution` support — works great over SSH jump hosts and inside **WSL2 + Windows Terminal**.
* **Auto Screen Detection & Dynamic Viewport (`F12` / `Ctrl+Y`):** Automatically detects physical window/monitor proportions (`TIOCGWINSZ` / `xrandr` / `DRM`) to eliminate vertical stretching, and lets you switch live between **Dynamic XRANDR** and fixed **Stretch (`1280x720`)** mode.
* **Instant Clipboard Bridge (`OSC 52` + Wayland):** Text copied (`Ctrl+C`) or selected inside the virtual X11 session automatically syncs to your local desktop (`wl-copy`) or mobile SSH client clipboard (`OSC 52` for Kitty, Alacritty, WezTerm, Foot, Windows Terminal, Termius, Termux).
* **Multithreaded C Zone-Diff Renderer:** Splits the frame into `16x8` zones, rendering only dirty regions via ANSI TrueColor half-blocks (`▀`) with Synchronized Output (`\x1b[?2026h`) to eliminate tearing and minimize SSH bandwidth.
* **Hardware VA-API Downscaling:** Uses `/dev/dri/renderD128` for GPU-accelerated frame scaling when available, falling back smoothly to fast bilinear CPU scaling.
* **Automatic Font & `1:1 (96 DPI)` Scaling:** Temporarily switches supported terminals (Ptyxis, Kitty, XTerm-compatible) to a crisp `4pt` pixel font (`TPlayPixel.ttf`), enforces `1:1 (96 DPI)` UI scaling inside `Xvfb` so complex apps like Blender and browsers fit cleanly, and restores your terminal upon exit.

---

## Hotkeys (Inside Session)

| Shortcut | Action |
| :--- | :--- |
| **`F12`** or **`Ctrl+Y`** | Toggle **Dynamic XRANDR** ↔ **Stretch (`1280x720`)** viewport mode |
| **`Ctrl+L`** | Force fit/maximize active windows and redraw screen |
| **`Ctrl+\`** | Exit session and restore terminal settings |

---

## Package Installation

### 1. Fedora / RHEL (`.rpm`)
Download `tplay-turbo-2.2.1-1.fc45.x86_64.rpm` from **[Releases](https://github.com)** and install via `dnf`:
```bash
sudo dnf install -y ./tplay-turbo-2.2.1-1.*.x86_64.rpm
```

### 2. Ubuntu / Debian / WSL2 (`.deb`)
Download `tplay-turbo_2.2.1_amd64.deb` from **[Releases](https://github.com)** and install via `apt`:
```bash
sudo apt update
sudo apt install -y ./tplay-turbo_2.2.1_amd64.deb
```

### 3. Universal Linux Tarball (`.tar.gz` — Arch, Gentoo, etc.)
Download `tplay-turbo-2.2.1-linux-x86_64.tar.gz` from **[Releases](https://github.com)**, extract, and run the installer:
```bash
tar -xzf tplay-turbo-2.2.1-linux-x86_64.tar.gz
cd tplay-turbo-2.2.1
./install.sh
```

### 4. Manual Build from Source
```bash
git clone https://github.com/m393m/tplay.git
cd tplay
./build.sh
sudo make install   
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
4. **Input & Clipboard Bridge:** Translates SGR mouse events (`1006`) and keyboard input via `xdotool`, while syncing X11 clipboard changes to your local terminal via `OSC 52` and `wl-copy`.

---

## Disclaimer & Stability Notice

> **Experimental Software Notice:** TPlay Turbo (`v2.2.1`) is an active experimental project under rapid development. While tested on Fedora, Ubuntu, and WSL2, you may encounter occasional visual artifacts, terminal font reset quirks on unsupported terminal emulators, high CPU usage at very large grid resolutions, or input edge cases in certain X11/OpenGL applications. Always save important work in external applications and press **`Ctrl+\`** to exit cleanly and restore terminal settings.

---

## License

MIT License
