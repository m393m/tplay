#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define ZONE_W 16
#define ZONE_H 8
#define NUM_THREADS 4

static uint8_t *prev_frame = NULL;
static int prev_w = 0;
static int prev_h = 0;

typedef struct {
    const uint8_t *frame;
    uint8_t *prev;
    int cols;
    int rows;
    int force_full;
    int zone_start;
    int zone_end;
    int zones_x;
    char *out_buf;
    int out_len;
    int out_cap;
} worker_arg_t;

static inline void append_str(worker_arg_t *w, const char *s, int len) {
    if (w->out_len + len >= w->out_cap) {
        w->out_cap = (w->out_cap + len) * 2 + 4096;
        w->out_buf = (char *)realloc(w->out_buf, w->out_cap);
    }
    memcpy(w->out_buf + w->out_len, s, len);
    w->out_len += len;
}

static void *render_worker(void *arg) {
    worker_arg_t *w = (worker_arg_t *)arg;
    int stride = w->cols * 3;
    char tmp[128];

    for (int z = w->zone_start; z < w->zone_end; z++) {
        int zx = z % w->zones_x;
        int zy = z / w->zones_x;

        int c0 = zx * ZONE_W;
        int r0 = zy * ZONE_H;
        int c1 = c0 + ZONE_W;
        int r1 = r0 + ZONE_H;
        if (c1 > w->cols) c1 = w->cols;
        if (r1 > w->rows) r1 = w->rows;

        int dirty = w->force_full;
        if (!dirty) {
            for (int r = r0; r < r1 && !dirty; r++) {
                int y_top = r * 2;
                int y_bot = y_top + 1;
                const uint8_t *cur_t = w->frame + y_top * stride + c0 * 3;
                const uint8_t *prv_t = w->prev + y_top * stride + c0 * 3;
                const uint8_t *cur_b = w->frame + y_bot * stride + c0 * 3;
                const uint8_t *prv_b = w->prev + y_bot * stride + c0 * 3;
                int row_bytes = (c1 - c0) * 3;
                if (memcmp(cur_t, prv_t, row_bytes) != 0 || memcmp(cur_b, prv_b, row_bytes) != 0) {
                    dirty = 1;
                }
            }
        }

        if (!dirty) continue;

        int last_tr = -1, last_tg = -1, last_tb = -1;
        int last_br = -1, last_bg = -1, last_bb = -1;

        for (int r = r0; r < r1; r++) {
            int n = snprintf(tmp, sizeof(tmp), "\x1b[%d;%dH", r + 1, c0 + 1);
            append_str(w, tmp, n);

            int y_top = r * 2;
            int y_bot = y_top + 1;
            const uint8_t *pt = w->frame + y_top * stride + c0 * 3;
            const uint8_t *pb = w->frame + y_bot * stride + c0 * 3;

            for (int c = c0; c < c1; c++) {
                int tr = pt[0], tg = pt[1], tb = pt[2];
                int br = pb[0], bg = pb[1], bb = pb[2];
                pt += 3;
                pb += 3;

                if (tr != last_tr || tg != last_tg || tb != last_tb) {
                    n = snprintf(tmp, sizeof(tmp), "\x1b[38;2;%d;%d;%dm", tr, tg, tb);
                    append_str(w, tmp, n);
                    last_tr = tr; last_tg = tg; last_tb = tb;
                }
                if (br != last_br || bg != last_bg || bb != last_bb) {
                    n = snprintf(tmp, sizeof(tmp), "\x1b[48;2;%d;%d;%dm", br, bg, bb);
                    append_str(w, tmp, n);
                    last_br = br; last_bg = bg; last_bb = bb;
                }
                append_str(w, "\xE2\x96\x80", 3);
            }
        }
    }
    return NULL;
}

int render_diff_frame(const uint8_t *frame, int cols, int rows, int force_full, uint8_t *out_buf) {
    int h = rows * 2;
    size_t frame_size = (size_t)cols * (size_t)h * 3;

    if (!prev_frame || prev_w != cols || prev_h != h) {
        free(prev_frame);
        prev_frame = (uint8_t *)malloc(frame_size);
        if (!prev_frame) return 0;
        prev_w = cols;
        prev_h = h;
        force_full = 1;
    }

    int zones_x = (cols + ZONE_W - 1) / ZONE_W;
    int zones_y = (rows + ZONE_H - 1) / ZONE_H;
    int total_zones = zones_x * zones_y;

    pthread_t threads[NUM_THREADS];
    worker_arg_t args[NUM_THREADS];
    int active_threads = total_zones < NUM_THREADS ? total_zones : NUM_THREADS;
    if (active_threads <= 0) return 0;

    int base = total_zones / active_threads;
    int rem = total_zones % active_threads;
    int cur = 0;

    for (int i = 0; i < active_threads; i++) {
        int count = base + (i < rem ? 1 : 0);
        args[i].frame = frame;
        args[i].prev = prev_frame;
        args[i].cols = cols;
        args[i].rows = rows;
        args[i].force_full = force_full;
        args[i].zone_start = cur;
        args[i].zone_end = cur + count;
        args[i].zones_x = zones_x;
        args[i].out_cap = 65536;
        args[i].out_buf = (char *)malloc(args[i].out_cap);
        args[i].out_len = 0;
        cur += count;
        pthread_create(&threads[i], NULL, render_worker, &args[i]);
    }

    int pos = 0;
    memcpy(out_buf + pos, "\x1b[?2026h", 8);
    pos += 8;

    for (int i = 0; i < active_threads; i++) {
        pthread_join(threads[i], NULL);
        if (args[i].out_len > 0) {
            memcpy(out_buf + pos, args[i].out_buf, args[i].out_len);
            pos += args[i].out_len;
        }
        free(args[i].out_buf);
    }

    memcpy(out_buf + pos, "\x1b[0m\x1b[?2026l", 12);
    pos += 12;

    memcpy(prev_frame, frame, frame_size);
    return pos;
}
