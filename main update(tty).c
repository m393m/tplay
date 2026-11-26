#define _POSIX_C_SOURCE 200809L
#define WLR_USE_UNSTABLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/xwayland/xwayland.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

struct tplay_server {
    struct wl_display *wl_display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_compositor *compositor;
    struct wlr_scene *scene;
    struct wlr_scene_output_layout *scene_layout;
    struct wlr_output_layout *output_layout;
    struct wlr_xdg_shell *xdg_shell;
    struct wlr_xwayland *xwayland;
    struct wlr_seat *seat;
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;
    struct wlr_relative_pointer_manager_v1 *rel_pointer_mgr;
    struct wlr_pointer_constraints_v1 *pointer_constraints;
    struct wlr_pointer_constraint_v1 *active_constraint;

    struct wl_list outputs;
    struct wl_list toplevels;
    struct wl_list keyboards;

    struct wl_listener new_output;
    struct wl_listener new_xdg_toplevel;
    struct wl_listener new_xwayland_surface;
    struct wl_listener xwayland_ready;
    struct wl_listener new_input;
    struct wl_listener new_constraint;
    struct wl_listener constraint_destroy;
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_abs;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;
    struct wl_listener request_cursor;

    struct wl_event_source *exit_timer;
    pid_t app_pid;
    int app_w;
    int app_h;
    bool terminating;
};

struct tplay_output {
    struct wl_list link;
    struct tplay_server *server;
    struct wlr_output *wlr_output;
    struct wl_listener frame;
    struct wl_listener destroy;
};

struct tplay_toplevel {
    struct wl_list link;
    struct tplay_server *server;
    struct wlr_xdg_toplevel *xdg_toplevel;
    struct wlr_scene_tree *scene_tree;
    struct wl_listener map;
    struct wl_listener commit;
    struct wl_listener destroy;
};

struct tplay_xwayland_view {
    struct tplay_server *server;
    struct wlr_xwayland_surface *xsurface;
    struct wlr_scene_tree *scene_tree;
    struct wl_listener associate;
    struct wl_listener dissociate;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener request_configure;
    struct wl_listener destroy;
};

struct tplay_keyboard {
    struct wl_list link;
    struct tplay_server *server;
    struct wlr_keyboard *wlr_keyboard;
    struct wl_listener modifiers;
    struct wl_listener key;
    struct wl_listener destroy;
};

static struct tplay_server *g_srv = NULL;
static volatile sig_atomic_t g_should_exit = 0;

static void request_safe_exit(struct tplay_server *srv) {
    if (!srv || srv->terminating) return;
    srv->terminating = true;
    wl_display_terminate(srv->wl_display);
}

static int exit_timer_cb(void *data) {
    struct tplay_server *srv = data;
    if (g_should_exit) {
        request_safe_exit(srv);
        return 0;
    }
    if (srv->app_pid > 0) {
        int status = 0;
        if (waitpid(srv->app_pid, &status, WNOHANG) == srv->app_pid) {
            srv->app_pid = -1;
            request_safe_exit(srv);
            return 0;
        }
    }
    wl_event_source_timer_update(srv->exit_timer, 50);
    return 0;
}

static void focus_surface(struct tplay_server *srv, struct wlr_surface *surface) {
    if (!srv || !surface) return;
    struct wlr_keyboard *kb = wlr_seat_get_keyboard(srv->seat);
    if (kb) {
        wlr_seat_keyboard_notify_enter(srv->seat, surface,
            kb->keycodes, kb->num_keycodes, &kb->modifiers);
    }
}

static void focus_toplevel(struct tplay_toplevel *tl) {
    if (!tl || !tl->xdg_toplevel || !tl->xdg_toplevel->base) return;
    struct tplay_server *srv = tl->server;
    wlr_scene_node_raise_to_top(&tl->scene_tree->node);
    wlr_xdg_toplevel_set_activated(tl->xdg_toplevel, true);
    if (srv->app_w > 0 && srv->app_h > 0) {
        wlr_xdg_toplevel_set_size(tl->xdg_toplevel, srv->app_w, srv->app_h);
    }
    focus_surface(srv, tl->xdg_toplevel->base->surface);
}

/* --- Клавиатура --- */
static void on_kbd_modifiers(struct wl_listener *listener, void *data) {
    (void)data;
    struct tplay_keyboard *kb = wl_container_of(listener, kb, modifiers);
    wlr_seat_set_keyboard(kb->server->seat, kb->wlr_keyboard);
    wlr_seat_keyboard_notify_modifiers(kb->server->seat, &kb->wlr_keyboard->modifiers);
}

static void on_kbd_key(struct wl_listener *listener, void *data) {
    struct tplay_keyboard *kb = wl_container_of(listener, kb, key);
    struct tplay_server *srv = kb->server;
    struct wlr_keyboard_key_event *ev = data;

    uint32_t keycode = ev->keycode + 8;
    const xkb_keysym_t *syms = NULL;
    int nsyms = xkb_state_key_get_syms(kb->wlr_keyboard->xkb_state, keycode, &syms);
    uint32_t mods = wlr_keyboard_get_modifiers(kb->wlr_keyboard);

    if (ev->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        bool ctrl = (mods & WLR_MODIFIER_CTRL) != 0;
        for (int i = 0; i < nsyms; i++) {
            xkb_keysym_t sym = *(syms + i);
            if (sym == XKB_KEY_F12 ||
                (ctrl && (sym == XKB_KEY_Delete || sym == XKB_KEY_BackSpace || sym == XKB_KEY_backslash))) {
                request_safe_exit(srv);
                return;
            }
        }
    }

    wlr_seat_set_keyboard(srv->seat, kb->wlr_keyboard);
    wlr_seat_keyboard_notify_key(srv->seat, ev->time_msec, ev->keycode, ev->state);
}

static void on_kbd_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct tplay_keyboard *kb = wl_container_of(listener, kb, destroy);
    wl_list_remove(&kb->modifiers.link);
    wl_list_remove(&kb->key.link);
    wl_list_remove(&kb->destroy.link);
    wl_list_remove(&kb->link);
    free(kb);
}

static void setup_keyboard(struct tplay_server *srv, struct wlr_input_device *dev) {
    struct wlr_keyboard *wlr_kb = wlr_keyboard_from_input_device(dev);
    struct tplay_keyboard *kb = calloc(1, sizeof(*kb));
    kb->server = srv;
    kb->wlr_keyboard = wlr_kb;

    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_rule_names rules = {
        .layout = "us,ru",
        .options = "grp:alt_shift_toggle",
    };
    struct xkb_keymap *keymap = xkb_keymap_new_from_names(ctx, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
        keymap = xkb_keymap_new_from_names(ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    }
    wlr_keyboard_set_keymap(wlr_kb, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(ctx);
    wlr_keyboard_set_repeat_info(wlr_kb, 25, 400);

    kb->modifiers.notify = on_kbd_modifiers;
    wl_signal_add(&wlr_kb->events.modifiers, &kb->modifiers);
    kb->key.notify = on_kbd_key;
    wl_signal_add(&wlr_kb->events.key, &kb->key);
    kb->destroy.notify = on_kbd_destroy;
    wl_signal_add(&dev->events.destroy, &kb->destroy);

    wlr_seat_set_keyboard(srv->seat, wlr_kb);
    wl_list_insert(&srv->keyboards, &kb->link);
}

/* --- Мышь / Курсор / 3D-камера --- */
static void process_cursor_motion(struct tplay_server *srv, uint32_t time_msec) {
    double sx = 0, sy = 0;
    struct wlr_surface *surface = NULL;
    struct wlr_scene_node *node = wlr_scene_node_at(&srv->scene->tree.node,
        srv->cursor->x, srv->cursor->y, &sx, &sy);

    if (node && node->type == WLR_SCENE_NODE_BUFFER) {
        struct wlr_scene_buffer *sbuf = wlr_scene_buffer_from_node(node);
        struct wlr_scene_surface *ssurf = wlr_scene_surface_try_from_buffer(sbuf);
        if (ssurf) surface = ssurf->surface;
    }

    if (!surface) {
        wlr_cursor_set_xcursor(srv->cursor, srv->cursor_mgr, "default");
        wlr_seat_pointer_clear_focus(srv->seat);
    } else {
        wlr_seat_pointer_notify_enter(srv->seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(srv->seat, time_msec, sx, sy);
    }
}

static void on_constraint_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct tplay_server *srv = wl_container_of(listener, srv, constraint_destroy);
    wl_list_remove(&srv->constraint_destroy.link);
    srv->active_constraint = NULL;
    wlr_cursor_set_xcursor(srv->cursor, srv->cursor_mgr, "default");
}

static void on_new_pointer_constraint(struct wl_listener *listener, void *data) {
    struct tplay_server *srv = wl_container_of(listener, srv, new_constraint);
    struct wlr_pointer_constraint_v1 *constraint = data;

    if (srv->active_constraint) {
        wl_list_remove(&srv->constraint_destroy.link);
        srv->active_constraint = NULL;
    }

    srv->active_constraint = constraint;
    srv->constraint_destroy.notify = on_constraint_destroy;
    wl_signal_add(&constraint->events.destroy, &srv->constraint_destroy);
    wlr_pointer_constraint_v1_send_activated(constraint);
}

static void on_cursor_motion(struct wl_listener *listener, void *data) {
    struct tplay_server *srv = wl_container_of(listener, srv, cursor_motion);
    struct wlr_pointer_motion_event *ev = data;

    wlr_relative_pointer_manager_v1_send_relative_motion(
        srv->rel_pointer_mgr, srv->seat,(uint64_t)ev->time_msec * 1000,
ev->delta_x, ev->delta_y,
ev->unaccel_dx, ev->unaccel_dy);
if (!srv->active_constraint) {
wlr_cursor_move(srv->cursor, &ev->pointer->base, ev->delta_x, ev->delta_y);
process_cursor_motion(srv, ev->time_msec);
}
}
static void on_cursor_motion_abs(struct wl_listener *listener, void *data) {
struct tplay_server *srv = wl_container_of(listener, srv, cursor_motion_abs);
struct wlr_pointer_motion_absolute_event *ev = data;
wlr_cursor_warp_absolute(srv->cursor, &ev->pointer->base, ev->x, ev->y);
process_cursor_motion(srv, ev->time_msec);
}
static void on_cursor_button(struct wl_listener *listener, void *data) {
struct tplay_server *srv = wl_container_of(listener, srv, cursor_button);
struct wlr_pointer_button_event *ev = data;
wlr_seat_pointer_notify_button(srv->seat, ev->time_msec, ev->button, ev->state);
}
static void on_cursor_axis(struct wl_listener *listener, void *data) {
struct tplay_server *srv = wl_container_of(listener, srv, cursor_axis);
struct wlr_pointer_axis_event *ev = data;
wlr_seat_pointer_notify_axis(srv->seat, ev->time_msec, ev->orientation,
ev->delta, ev->delta_discrete, ev->source, ev->relative_direction);
}
static void on_cursor_frame(struct wl_listener *listener, void *data) {
(void)data;
struct tplay_server *srv = wl_container_of(listener, srv, cursor_frame);
wlr_seat_pointer_notify_frame(srv->seat);
}
static void on_request_cursor(struct wl_listener *listener, void *data) {
struct tplay_server *srv = wl_container_of(listener, srv, request_cursor);
struct wlr_seat_pointer_request_set_cursor_event *ev = data;
if (srv->seat->pointer_state.focused_client == ev->seat_client) {
wlr_cursor_set_surface(srv->cursor, ev->surface, ev->hotspot_x, ev->hotspot_y);
}
}
static void on_new_input(struct wl_listener *listener, void *data) {
struct tplay_server *srv = wl_container_of(listener, srv, new_input);
struct wlr_input_device *dev = data;
if (dev->type == WLR_INPUT_DEVICE_KEYBOARD) {
setup_keyboard(srv, dev);
} else if (dev->type == WLR_INPUT_DEVICE_POINTER) {
wlr_cursor_attach_input_device(srv->cursor, dev);
}
uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
if (!wl_list_empty(&srv->keyboards)) {
caps |= WL_SEAT_CAPABILITY_KEYBOARD;
}
wlr_seat_set_capabilities(srv->seat, caps);
}
/* --- Нативные окна Wayland (XDG Toplevel) --- */
static void on_toplevel_map(struct wl_listener *listener, void *data) {
(void)data;
struct tplay_toplevel *tl = wl_container_of(listener, tl, map);
wl_list_insert(&tl->server->toplevels, &tl->link);
focus_toplevel(tl);
process_cursor_motion(tl->server, 0);
}
static void on_toplevel_commit(struct wl_listener *listener, void *data) {
(void)data;
struct tplay_toplevel *tl = wl_container_of(listener, tl, commit);
if (tl->xdg_toplevel->base->initial_commit && tl->server->app_w > 0) {
wlr_xdg_toplevel_set_size(tl->xdg_toplevel, tl->server->app_w, tl->server->app_h);
}
}
static void on_toplevel_destroy(struct wl_listener *listener, void *data) {
(void)data;
struct tplay_toplevel *tl = wl_container_of(listener, tl, destroy);
wl_list_remove(&tl->map.link);
wl_list_remove(&tl->commit.link);
wl_list_remove(&tl->destroy.link);
wl_list_remove(&tl->link);
free(tl);
}
static void on_new_xdg_toplevel(struct wl_listener *listener, void *data) {
struct tplay_server *srv = wl_container_of(listener, srv, new_xdg_toplevel);
struct wlr_xdg_toplevel *xdg_tl = data;
struct tplay_toplevel *tl = calloc(1, sizeof(*tl));
tl->server = srv;
tl->xdg_toplevel = xdg_tl;
tl->scene_tree = wlr_scene_xdg_surface_create(&srv->scene->tree, xdg_tl->base);
tl->scene_tree->node.data = tl;
xdg_tl->base->data = tl->scene_tree;
wl_list_init(&tl->link);
tl->map.notify = on_toplevel_map;
wl_signal_add(&xdg_tl->base->surface->events.map, &tl->map);
tl->commit.notify = on_toplevel_commit;
wl_signal_add(&xdg_tl->base->surface->events.commit, &tl->commit);
tl->destroy.notify = on_toplevel_destroy;
wl_signal_add(&xdg_tl->events.destroy, &tl->destroy);
}
/* --- Окна X11 (резервный мост XWayland для старых программ и Wine) --- */
static void on_xwayland_map(struct wl_listener *listener, void *data) {
(void)data;
struct tplay_xwayland_view *xv = wl_container_of(listener, xv, map);
struct tplay_server *srv = xv->server;
if (!xv->scene_tree && xv->xsurface->surface) {
xv->scene_tree = wlr_scene_subsurface_tree_create(&srv->scene->tree, xv->xsurface->surface);
}
if (xv->scene_tree) {
wlr_scene_node_set_enabled(&xv->scene_tree->node, true);
wlr_scene_node_raise_to_top(&xv->scene_tree->node);
if (!xv->xsurface->override_redirect && srv->app_w > 0 && srv->app_h > 0) {
wlr_xwayland_surface_configure(xv->xsurface, 0, 0,
(uint16_t)srv->app_w, (uint16_t)srv->app_h);
wlr_scene_node_set_position(&xv->scene_tree->node, 0, 0);
} else {
wlr_scene_node_set_position(&xv->scene_tree->node, xv->xsurface->x, xv->xsurface->y);
}
}
wlr_xwayland_surface_activate(xv->xsurface, true);
focus_surface(srv, xv->xsurface->surface);
process_cursor_motion(srv, 0);
}
static void on_xwayland_unmap(struct wl_listener *listener, void *data) {
(void)data;
struct tplay_xwayland_view *xv = wl_container_of(listener, xv, unmap);
if (xv->scene_tree) {
wlr_scene_node_set_enabled(&xv->scene_tree->node, false);
}
}
static void on_xwayland_associate(struct wl_listener *listener, void *data) {
(void)data;
struct tplay_xwayland_view *xv = wl_container_of(listener, xv, associate);
xv->map.notify = on_xwayland_map;
wl_signal_add(&xv->xsurface->surface->events.map, &xv->map);
xv->unmap.notify = on_xwayland_unmap;
wl_signal_add(&xv->xsurface->surface->events.unmap, &xv->unmap);
}
static void on_xwayland_dissociate(struct wl_listener *listener, void *data) {
(void)data;
struct tplay_xwayland_view *xv = wl_container_of(listener, xv, dissociate);
wl_list_remove(&xv->map.link);
wl_list_remove(&xv->unmap.link);
if (xv->scene_tree) {
wlr_scene_node_destroy(&xv->scene_tree->node);
xv->scene_tree = NULL;
}
}
static void on_xwayland_request_configure(struct wl_listener *listener, void *data) {
struct tplay_xwayland_view *xv = wl_container_of(listener, xv, request_configure);
struct wlr_xwayland_surface_configure_event *ev = data;
struct tplay_server *srv = xv->server;
if (!xv->xsurface->override_redirect && srv->app_w > 0 && srv->app_h > 0) {
wlr_xwayland_surface_configure(xv->xsurface, 0, 0,
(uint16_t)srv->app_w, (uint16_t)srv->app_h);
} else {
wlr_xwayland_surface_configure(xv->xsurface, ev->x, ev->y, ev->width, ev->height);
}
}
static void on_xwayland_destroy(struct wl_listener *listener, void *data) {
(void)data;
struct tplay_xwayland_view *xv = wl_container_of(listener, xv, destroy);
wl_list_remove(&xv->associate.link);
wl_list_remove(&xv->dissociate.link);
wl_list_remove(&xv->request_configure.link);
wl_list_remove(&xv->destroy.link);
free(xv);
}
static void on_new_xwayland_surface(struct wl_listener *listener, void *data) {
struct tplay_server *srv = wl_container_of(listener, srv, new_xwayland_surface);
struct wlr_xwayland_surface *xsurface = data;
struct tplay_xwayland_view *xv = calloc(1, sizeof(*xv));
xv->server = srv;
xv->xsurface = xsurface;
xv->associate.notify = on_xwayland_associate;
wl_signal_add(&xsurface->events.associate, &xv->associate);
xv->dissociate.notify = on_xwayland_dissociate;
wl_signal_add(&xsurface->events.dissociate, &xv->dissociate);
xv->request_configure.notify = on_xwayland_request_configure;
wl_signal_add(&xsurface->events.request_configure, &xv->request_configure);
xv->destroy.notify = on_xwayland_destroy;
wl_signal_add(&xsurface->events.destroy, &xv->destroy);
}
static void on_xwayland_ready(struct wl_listener *listener, void *data) {
(void)data;
struct tplay_server *srv = wl_container_of(listener, srv, xwayland_ready);
wlr_xwayland_set_seat(srv->xwayland, srv->seat);
}
/* --- Монитор / DRM Output --- */
static void on_output_frame(struct wl_listener *listener, void *data) {
(void)data;
struct tplay_output *out = wl_container_of(listener, out, frame);
struct wlr_scene_output *scene_out = wlr_scene_get_scene_output(out->server->scene, out->wlr_output);
if (!scene_out) return;
wlr_scene_output_commit(scene_out, NULL);
struct timespec now;
clock_gettime(CLOCK_MONOTONIC, &now);
wlr_scene_output_send_frame_done(scene_out, &now);
}
static void on_output_destroy(struct wl_listener *listener, void *data) {
(void)data;
struct tplay_output *out = wl_container_of(listener, out, destroy);
wl_list_remove(&out->frame.link);
wl_list_remove(&out->destroy.link);
wl_list_remove(&out->link);
free(out);
}
static void on_new_output(struct wl_listener *listener, void *data) {
struct tplay_server *srv = wl_container_of(listener, srv, new_output);
struct wlr_output *wlr_out = data;
wlr_output_init_render(wlr_out, srv->allocator, srv->renderer);
struct wlr_output_state state;
wlr_output_state_init(&state);
wlr_output_state_set_enabled(&state, true);
struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_out);
if (mode) {
wlr_output_state_set_mode(&state, mode);
srv->app_w = mode->width;
srv->app_h = mode->height;
} else {
srv->app_w = wlr_out->width > 0 ? wlr_out->width : 1920;
srv->app_h = wlr_out->height > 0 ? wlr_out->height : 1200;
}
wlr_output_commit_state(wlr_out, &state);
wlr_output_state_finish(&state);
struct tplay_output *out = calloc(1, sizeof(*out));
out->server = srv;
out->wlr_output = wlr_out;
out->frame.notify = on_output_frame;
wl_signal_add(&wlr_out->events.frame, &out->frame);
out->destroy.notify = on_output_destroy;
wl_signal_add(&wlr_out->events.destroy, &out->destroy);
wl_list_insert(&srv->outputs, &out->link);
struct wlr_output_layout_output *l_out = wlr_output_layout_add_auto(srv->output_layout, wlr_out);
struct wlr_scene_output *s_out = wlr_scene_output_create(srv->scene, wlr_out);
wlr_scene_output_layout_add_output(srv->scene_layout, l_out, s_out);
}
static void on_sigchld(int sig) { (void)sig; }
static void on_term_sig(int sig) {
(void)sig;
g_should_exit = 1;
}
int main(int argc, char **argv) {
if (argc < 2) {
fprintf(stderr, "Использование: %s <программа> [аргументы...]\nВыход: Ctrl+Delete, Ctrl+BackSpace или F12\n", *argv);
return 1;
}
    unsigned uid = (unsigned)getuid();
    char *rt_dir = malloc(128);
    char *dbus_addr = malloc(160);
    if (rt_dir && dbus_addr) {
        snprintf(rt_dir, 128, "/run/user/%u", uid);
        snprintf(dbus_addr, 160, "unix:path=/run/user/%u/bus", uid);
        if (access(rt_dir, F_OK) == 0) {
            setenv("XDG_RUNTIME_DIR", rt_dir, 1);
            setenv("DBUS_SESSION_BUS_ADDRESS", dbus_addr, 1);
        }
    }
    free(rt_dir);
    free(dbus_addr);

        wlr_log_init(WLR_ERROR, NULL);
struct tplay_server srv = {0};
g_srv = &srv;
wl_list_init(&srv.outputs);
wl_list_init(&srv.toplevels);
wl_list_init(&srv.keyboards);
srv.wl_display = wl_display_create();
if (!srv.wl_display) return 1;
struct wl_event_loop *loop = wl_display_get_event_loop(srv.wl_display);
srv.backend = wlr_backend_autocreate(loop, NULL);
if (!srv.backend) {
fprintf(stderr, "[Ошибка DRM/Wayland] Запустите из чистого TTY (Ctrl+Alt+F4): tplay %s\n", *(argv + 1));
wl_display_destroy(srv.wl_display);
return 1;
}
srv.renderer = wlr_renderer_autocreate(srv.backend);
wlr_renderer_init_wl_display(srv.renderer, srv.wl_display);
srv.allocator = wlr_allocator_autocreate(srv.backend, srv.renderer);
srv.compositor = wlr_compositor_create(srv.wl_display, 5, srv.renderer);
wlr_subcompositor_create(srv.wl_display);
wlr_data_device_manager_create(srv.wl_display);
srv.output_layout = wlr_output_layout_create(srv.wl_display);
srv.scene = wlr_scene_create();
srv.scene_layout = wlr_scene_attach_output_layout(srv.scene, srv.output_layout);
srv.xdg_shell = wlr_xdg_shell_create(srv.wl_display, 3);
srv.new_xdg_toplevel.notify = on_new_xdg_toplevel;
wl_signal_add(&srv.xdg_shell->events.new_toplevel, &srv.new_xdg_toplevel);
srv.cursor = wlr_cursor_create();
wlr_cursor_attach_output_layout(srv.cursor, srv.output_layout);
srv.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
srv.cursor_motion.notify = on_cursor_motion;
wl_signal_add(&srv.cursor->events.motion, &srv.cursor_motion);
srv.cursor_motion_abs.notify = on_cursor_motion_abs;
wl_signal_add(&srv.cursor->events.motion_absolute, &srv.cursor_motion_abs);
srv.cursor_button.notify = on_cursor_button;
wl_signal_add(&srv.cursor->events.button, &srv.cursor_button);
srv.cursor_axis.notify = on_cursor_axis;
wl_signal_add(&srv.cursor->events.axis, &srv.cursor_axis);
srv.cursor_frame.notify = on_cursor_frame;
wl_signal_add(&srv.cursor->events.frame, &srv.cursor_frame);
srv.seat = wlr_seat_create(srv.wl_display, "seat0");
srv.request_cursor.notify = on_request_cursor;
wl_signal_add(&srv.seat->events.request_set_cursor, &srv.request_cursor);
srv.rel_pointer_mgr = wlr_relative_pointer_manager_v1_create(srv.wl_display);
srv.pointer_constraints = wlr_pointer_constraints_v1_create(srv.wl_display);
srv.new_constraint.notify = on_new_pointer_constraint;
wl_signal_add(&srv.pointer_constraints->events.new_constraint, &srv.new_constraint);
srv.new_output.notify = on_new_output;
wl_signal_add(&srv.backend->events.new_output, &srv.new_output);
srv.new_input.notify = on_new_input;
wl_signal_add(&srv.backend->events.new_input, &srv.new_input);
const char *socket = wl_display_add_socket_auto(srv.wl_display);
if (socket) {
    setenv("WAYLAND_DISPLAY", socket, 1);
    unsetenv("DISPLAY");
}
if (!socket || !wlr_backend_start(srv.backend)) {
wlr_backend_destroy(srv.backend);
wl_display_destroy(srv.wl_display);
return 1;
}
/* Гибридный мост XWayland запускается ПОСЛЕ открытия Wayland-сокета и старта бэкенда */
srv.xwayland = wlr_xwayland_create(srv.wl_display, srv.compositor, false);
if (srv.xwayland) {
srv.new_xwayland_surface.notify = on_new_xwayland_surface;
wl_signal_add(&srv.xwayland->events.new_surface, &srv.new_xwayland_surface);
srv.xwayland_ready.notify = on_xwayland_ready;
wl_signal_add(&srv.xwayland->events.ready, &srv.xwayland_ready);
}
if (0) {
if (srv.xwayland) wlr_xwayland_destroy(srv.xwayland);
wlr_backend_destroy(srv.backend);
wl_display_destroy(srv.wl_display);
return 1;
}
wlr_cursor_warp(srv.cursor, NULL, srv.app_w / 2.0, srv.app_h / 2.0);
wlr_cursor_set_xcursor(srv.cursor, srv.cursor_mgr, "default");
signal(SIGCHLD, on_sigchld);
signal(SIGINT, on_term_sig);
signal(SIGTERM, on_term_sig);
srv.exit_timer = wl_event_loop_add_timer(loop, exit_timer_cb, &srv);
wl_event_source_timer_update(srv.exit_timer, 50);
srv.app_pid = fork();
if (srv.app_pid == 0) {
setenv("WAYLAND_DISPLAY", socket, 1);
setenv("XDG_SESSION_TYPE", "wayland", 1);
setenv("GDK_BACKEND", "wayland", 1);
setenv("QT_QPA_PLATFORM", "wayland", 1);
setenv("SDL_VIDEODRIVER", "wayland", 1);
setenv("MOZ_ENABLE_WAYLAND", "1", 1);
if (srv.xwayland && srv.xwayland->display_name) {
setenv("DISPLAY", srv.xwayland->display_name, 1);
} else {
unsetenv("DISPLAY");
}
unsigned uid = (unsigned)getuid();
char *rt_dir = malloc(128);
if (rt_dir) {
snprintf(rt_dir, 128, "/run/user/%u", uid);
if (access(rt_dir, F_OK) == 0) {
setenv("XDG_RUNTIME_DIR", rt_dir, 1);
char *dbus_addr = malloc(160);
if (dbus_addr) {
    snprintf(dbus_addr, 160, "unix:path=/run/user/%u/bus", uid);
    setenv("DBUS_SESSION_BUS_ADDRESS", dbus_addr, 1);
    free(dbus_addr);
}
}
free(rt_dir);
}
int log_fd = open("/dev/null", O_WRONLY);
if (log_fd >= 0) {
dup2(log_fd, STDOUT_FILENO);
dup2(log_fd, STDERR_FILENO);
close(log_fd);
}
execvp(*(argv + 1), argv + 1);
perror("execvp failed");
_exit(127);
}
wl_display_run(srv.wl_display);
wl_event_source_remove(srv.exit_timer);
if (srv.app_pid > 0) {
kill(srv.app_pid, SIGKILL);
waitpid(srv.app_pid, NULL, 0);
}
if (srv.xwayland) {
wl_list_remove(&srv.new_xwayland_surface.link);
wl_list_remove(&srv.xwayland_ready.link);
wlr_xwayland_destroy(srv.xwayland);
}
if (srv.active_constraint) {
wl_list_remove(&srv.constraint_destroy.link);
srv.active_constraint = NULL;
}
wl_list_remove(&srv.new_constraint.link);
wl_list_remove(&srv.new_xdg_toplevel.link);
wl_list_remove(&srv.new_output.link);
wl_list_remove(&srv.new_input.link);
wl_list_remove(&srv.cursor_motion.link);
wl_list_remove(&srv.cursor_motion_abs.link);
wl_list_remove(&srv.cursor_button.link);
wl_list_remove(&srv.cursor_axis.link);
wl_list_remove(&srv.cursor_frame.link);
wl_list_remove(&srv.request_cursor.link);
wl_display_destroy_clients(srv.wl_display);
wlr_backend_destroy(srv.backend);
wlr_scene_node_destroy(&srv.scene->tree.node);
wlr_xcursor_manager_destroy(srv.cursor_mgr);
wlr_cursor_destroy(srv.cursor);
wlr_allocator_destroy(srv.allocator);
wlr_renderer_destroy(srv.renderer);
wl_display_destroy(srv.wl_display);
return 0;
}
