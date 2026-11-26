# TPLAY DUAL-ARCHITECTURE SPECIFICATION & ENGINEERING RULES
PROJECT:Two-edition Linux graphical runner without heavy DE(GNOME/KDE) or standalone Xorg.
RELEASES_MODEL:
1.EDITION_TEXT_SSH(v2-text):ANSI Unicode half-block(U+2580) terminal renderer for remote SSH sessions,headless servers,and nested terminal emulators without DRM access.
2.EDITION_ADVANCED_NATIVE(v4.0.0,binary:tplay):Zero-config single-command launcher(tplay <app>) built as a hybrid C11 Wayland/DRM + XWayland compositor for local TTY execution with full GPU acceleration,audio,and 360-degree 3D input.
---
# ARCHITECTURE_1:TEXT_SSH_EDITION(v2-text)
PIPELINE:TargetApp->Xvfb(:99,virtual framebuffer)->FFmpeg(x11grab,rawvideo rgb24,lanczos scale to terminal cols x rows*2)->C_Renderer(stdin RGB24 stream->ANSI 24-bit TrueColor escape sequences \x1b[38;2;R;G;Bm\x1b[48;2;R;G;Bm▀ with delta frame diffing)->stdout(TTY/SSH).
INPUT_TEXT_MODE:Raw termios stdin(ECHO/ICANON/ISIG off)->ANSI escape parser(arrows,Fn,UTF-8,mouse SGR \x1b[<b;x;yM)->xdotool/XTest injection into Xvfb;exit on Ctrl+\(0x1c).
RULES_TEXT_MODE:Keep self-contained for SSH/headless environments;never require /dev/dri/card* in Text Edition.
---
# ARCHITECTURE_2:ADVANCED_NATIVE_EDITION(v4.0.0,main.c)
PIPELINE:TargetApp->Wayland_Socket(wayland-1) OR XWayland(:display)->wlroots-0.20_SceneGraph(wlr_scene)->EGL/GBM_ZeroCopy_Allocator->DRM/KMS_Hardware_Scanout(eDP/HDMI/DP @ native Hz).
COMPONENTS:
-BACKEND:wlr_backend_autocreate(loop,NULL) via systemd-logind/libseat(safe VT switching Ctrl+Alt+F2..F4,never call KDSETMODE/KD_GRAPHICS).
-RENDERER:wlr_renderer_autocreate + wlr_allocator_autocreate(DMA-BUF/Vulkan/GLES2 hardware acceleration on AMD/Intel/NVIDIA).
-SHELL_WAYLAND:wlr_xdg_shell(v3)->on_new_xdg_toplevel->auto-fullscreen(wlr_xdg_toplevel_set_size to output mode width/height,wlr_xdg_toplevel_set_activated true,wlr_scene_node_raise_to_top).
-SHELL_XWAYLAND:wlr_xwayland_create(wl_display,compositor,false) initialized strictly AFTER wl_display_add_socket_auto() and setenv("WAYLAND_DISPLAY",socket,1);handles associate/dissociate/map/unmap/request_configure;auto-maximizes non-override_redirect X11 windows(Godot3,Wine,legacy X11) while preserving popup positions(override_redirect).
-INPUT_KBD:libinput via wlr_seat->xkb_keymap(layout="us,ru",options="grp:alt_shift_toggle",repeat 25Hz/400ms);exit hotkeys=XKB_KEY_F12 or WLR_MODIFIER_CTRL+(XKB_KEY_Delete|XKB_KEY_BackSpace|XKB_KEY_backslash).
-INPUT_MOUSE_3D:wlr_cursor + wlr_xcursor_manager("default",24) + wlr_relative_pointer_manager_v1 + wlr_pointer_constraints_v1;on_new_pointer_constraint activates lock and binds constraint_destroy;in on_cursor_motion always send wlr_relative_pointer_manager_v1_send_relative_motion,and call wlr_cursor_move ONLY when active_constraint==NULL(enables 360-degree camera in 3D games and restores free cursor in pause menus).
-PROCESS_AND_ENV:fork/execvp target app with WAYLAND_DISPLAY=<socket>,DISPLAY=<xwayland_display>,XDG_SESSION_TYPE=wayland,GDK_BACKEND=wayland,QT_QPA_PLATFORM=wayland,SDL_VIDEODRIVER=wayland,MOZ_ENABLE_WAYLAND=1,XDG_RUNTIME_DIR=/run/user/<uid>,DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/<uid>/bus;app stdout/stderr logged to /tmp/tplay_c_app.log.
-LIFECYCLE_AND_CLEANUP:Never terminate from raw SIGCHLD(Xwayland child exits trigger SIGCHLD);poll waitpid(app_pid,WNOHANG) via 50ms wl_event_source timer;on shutdown remove all wl_listener links(xwayland,constraint,xdg_toplevel,output,input,cursor) and destroy xwayland->clients->backend->scene->cursor->allocator->renderer->display in exact order to prevent wl_list_empty asserts.
-SYSTEM_GROUPS:User must belong to audio and input groups for TTY PipeWire/PulseAudio and /dev/uinput gamepad(PhantomPad/Sober) access.
-STRICT_HYGIENE:Zero personal/home paths in source,Makefile,specs,or docs;compile cleanly with gcc -O3 -march=native -Wall -Wextra.
