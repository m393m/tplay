# tplay# TPlay Turbo

**TPlay Turbo** is an ultra-lightweight headless terminal graphics compositor and interactive TUI launcher for Linux. It allows you to run full-fledged GUI applications, games, and web browsers directly inside a text terminal (TTY or terminal emulator).

---

## What TPlay Turbo Can Launch

From a single interactive terminal menu (or directly via the command line), you can launch almost any type of Linux application:

* **Native X11, OpenGL, and Vulkan Applications** — standard desktop software and 3D apps rendered off-screen via a virtual framebuffer (`Xvfb`).
* **Games and 3D Graphics** — hardware-accelerated gaming and micro-compositing via `gamescope` and `VirtualGL`.
* **Containerized & Portable Packages:**
  * **Flatpak** (with automatic discovery of installed apps)
  * **Snap**
  * **AppImage** (direct execution of portable binaries)
* **Frameless Web Apps** — launch websites, browser games, and web services in clean kiosk mode without browser toolbars.

### Core Technologies Under the Hood
* **Multithreaded Zone-Based Diff Rendering (16×8):** A custom C shared library (`libtplay_turbo.so`) splits frames into zones and redraws only changed regions, drastically reducing CPU usage and boosting FPS.
* **Hardware VA-API Downscaling:** Real-time frame scaling and noise suppression powered by `ffmpeg`.
* **Smart Font Lifecycle Management:** Automatically switches to the bundled `TPlayPixel` font for crisp pixel-accurate graphics and cleanly resets terminal settings upon exit.

---

## Dependencies & Recommended Software

### Required Libraries & Packages
When installing via the `.rpm` package, all required dependencies are resolved automatically. For manual setups, ensure the following packages are installed:
* `python3` — core application logic and TUI interface.
* `ffmpeg` — screen capture, hardware VA-API downscaling, and frame filtering.
* `xorg-x11-server-Xvfb` — virtual X server for headless off-screen rendering.
* `xdotool` — keyboard and mouse input forwarding to the target window.
* `fontconfig` — registration and caching of the `TPlayPixel.ttf` font.

### Recommended Terminals & Accelerators (Optional)
For the smoothest frame rates, 24-bit TrueColor support, and lowest input latency, the following tools are recommended:
* **`kmscon`** *(Strongly Recommended)* — a modern KMS/DRM console terminal with hardware-accelerated font rendering. Ideal for running `tplay-turbo` on a dedicated TTY (e.g., `TTY3`) with a small font size (`font-size=8`).
* **GPU-Accelerated Terminal Emulators** — Alacritty, Kitty, Foot, or WezTerm (when running inside an existing desktop session).
* **`VirtualGL`** and **`gamescope`** — for hardware-accelerated OpenGL/Vulkan passthrough in demanding 3D games.

---

## Installation (Fedora / RPM)

1. Go to the **[Releases](https://github.com)** page and download the latest `.rpm` package (`tplay-turbo-2.0.0-1.*.x86_64.rpm`).
2. Install it using `dnf`:

```bash
sudo dnf install ./tplay-turbo-2.0.0-1.*.x86_64.rpm
```

3. *(Optional)* Install the recommended packages for maximum performance:

```bash
sudo dnf install kmscon VirtualGL gamescope
```

---

## How to Use

### 1. Interactive TUI Launcher
Run the command without arguments in your terminal:
```bash
tplay-turbo
```
An interactive menu will open where you can browse and launch installed apps (native, Flatpak, Snap, AppImage) or enter a custom command/URL.

### 2. Recommended Setup: Dedicated TTY with `kmscon`
For maximum resolution and speed without running a heavy desktop environment, configure `kmscon` on `TTY3` (`Ctrl + Alt + F3`) using the built-in `TPlayPixel` font:

```bash
sudo mkdir -p /etc/kmscon
cat << 'EOF' | sudo tee /etc/kmscon/kmscon.conf
font-name=TPlayPixel
font-size=8
drm
hwaccel
xkb-layout=us,ru
xkb-options=grp:alt_shift_toggle
EOF
```
Once `kmsconvt@tty3.service` is enabled, switch to `TTY3` (`Ctrl + Alt + F3`) and run `tplay-turbo`.
