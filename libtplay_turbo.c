#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define ZONE_W 16
#define ZONE_H 8
#define MAX_THREADS 16

static uint8_t *prev_frame = NULL;
static int prev_w = 0;
static int prev_h = 0;

typedef struct {
    const uint8_t *frame;
    uint8_t *out;
    int width;
    int height;
    int rows;
    int cols;
    int z_cols;
    int z_rows;
    int z_start;
    int z_end;
    int force;
    int bytes_written;
} WorkerArgs;

static inline int write_int(uint8_t *buf, int val) {
    if (val >= 100) {
        buf[0] = '0' + (val / 100);
        buf[1] = '0' + ((val / 10) % 10);
        buf[2] = '0' + (val % 10);
        return 3;
    } else if (val >= 10) {
        buf[0] = '0' + (val / 10);
        buf[1] = '0' + (val % 10);
        return 2;
    } else {
        buf[0] = '0' + val;
        return 1;
    }
}

static inline int write_pos(uint8_t *buf, int row, int col) {
    int p = 0;
    buf[p++] = 0x1b;
    buf[p++] = '[';
    p += write_int(buf + p, row);
    buf[p++] = ';';
    p += write_int(buf + p, col);
    buf[p++] = 'H';
    return p;
}

static void *render_worker(void *arg) {
    WorkerArgs *w = (WorkerArgs *)arg;
    uint8_t *out = w->out;
    int pos = 0;
    int stride = w->width * 3;

    for (int z = w->z_start; z < w->z_end; z++) {
        int zy = z / w->z_cols;
        int zx = z % w->z_cols;

        int c0 = zx * ZONE_W;
        int c1 = c0 + ZONE_W;
        if (c1 > w->cols) c1 = w->cols;

        int r0 = zy * ZONE_H;
        int r1 = r0 + ZONE_H;
        if (r1 > w->rows) r1 = w->rows;

        int dirty = w->force;
        if (!dirty) {
            for (int r = r0; r < r1 && !dirty; r++) {
                int y_top = r * 2;
                int y_bot = y_top + 1;
                int off_top = y_top * stride + c0 * 3;
                int len = (c1 - c0) * 3;
                if (memcmp(w->frame + off_top, prev_frame + off_top, len) != 0) {
                    dirty = 1;
                    break;
                }
                if (y_bot < w->height) {
                    int off_bot = y_bot * stride + c0 * 3;
                    if (memcmp(w->frame + off_bot, prev_frame + off_bot, len) != 0) {
                        dirty = 1;
                        break;
                    }
                }
            }
        }

        if (!dirty) continue;

        int last_fg_r = -1, last_fg_g = -1, last_fg_b = -1;
        int last_bg_r = -1, last_bg_g = -1, last_bg_b = -1;

        for (int r = r0; r < r1; r++) {
            pos += write_pos(out + pos, r + 1, c0 + 1);
            int y_top = r * 2;
            int y_bot = y_top + 1;

            for (int c = c0; c < c1; c++) {
                const uint8_t *pt = w->frame + y_top * stride + c * 3;
                int fg_r = pt[0], fg_g = pt[1], fg_b = pt[2];

                int bg_r = 0, bg_g = 0, bg_b = 0;
                if (y_bot < w->height) {
                    const uint8_t *pb = w->frame + y_bot * stride + c * 3;
                    bg_r = pb[0]; bg_g = pb[1]; bg_b = pb[2];
                }

                if (fg_r != last_fg_r || fg_g != last_fg_g || fg_b != last_fg_b) {
                    memcpy(out + pos, "\x1b[38;2;", 7); pos += 7;
                    pos += write_int(out + pos, fg_r); out[pos++] = ';';
                    pos += write_int(out + pos, fg_g); out[pos++] = ';';
                    pos += write_int(out + pos, fg_b); out[pos++] = 'm';
                    last_fg_r = fg_r; last_fg_g = fg_g; last_fg_b = fg_b;
                }

                if (bg_r != last_bg_r || bg_g != last_bg_g || bg_b != last_bg_b) {
                    memcpy(out + pos, "\x1b[48;2;", 7); pos += 7;
                    pos += write_int(out + pos, bg_r); out[pos++] = ';';
                    pos += write_int(out + pos, bg_g); out[pos++] = ';';
                    pos += write_int(out + pos, bg_b); out[pos++] = 'm';
                    last_bg_r = bg_r; last_bg_g = bg_g; last_bg_b = bg_b;
                }

                out[pos++] = 0xE2;
                out[pos++] = 0x96;
                out[pos++] = 0x80;
            }
        }
    }

    w->bytes_written = pos;
    return NULL;
}

int render_diff_frame(const uint8_t *frame, int width, int height, int force_redraw, uint8_t *out_buf) {
    if (!frame || !out_buf || width <= 0 || height <= 0) return 0;

    size_t frame_bytes = (size_t)width * (size_t)height * 3;
    int force = force_redraw;

    if (!prev_frame || prev_w != width || prev_h != height) {
        free(prev_frame);
        prev_frame = (uint8_t *)malloc(frame_bytes);
        if (!prev_frame) return 0;
        prev_w = width;
        prev_h = height;
        force = 1;
    }

    int cols = width;
    int rows = (height + 1) / 2;
    int z_cols = (cols + ZONE_W - 1) / ZONE_W;
    int z_rows = (rows + ZONE_H - 1) / ZONE_H;
    int total_zones = z_cols * z_rows;

    int num_threads = total_zones < MAX_THREADS ? total_zones : MAX_THREADS;
    if (num_threads < 1) num_threads = 1;

    pthread_t threads[MAX_THREADS];
    WorkerArgs args[MAX_THREADS];

    size_t per_thread_cap = (frame_bytes * 24) / (size_t)num_threads + 4096;
    int zones_per_thread = (total_zones + num_threads - 1) / num_threads;

    for (int t = 0; t < num_threads; t++) {
        int z_start = t * zones_per_thread;
        int z_end = z_start + zones_per_thread;
        if (z_end > total_zones) z_end = total_zones;

        args[t].frame = frame;
        args[t].out = (uint8_t *)malloc(per_thread_cap);
        args[t].width = width;
        args[t].height = height;
        args[t].rows = rows;
        args[t].cols = cols;
        args[t].z_cols = z_cols;
        args[t].z_rows = z_rows;
        args[t].z_start = z_start;
        args[t].z_end = z_end;
        args[t].force = force;
        args[t].bytes_written = 0;

        if (z_start < z_end) {
            pthread_create(&threads[t], NULL, render_worker, &args[t]);
        }
    }

    int total_out = 0;
    memcpy(out_buf + total_out, "\x1b[?2026h", 8);
    total_out += 8;

    for (int t = 0; t < num_threads; t++) {
        if (args[t].z_start < args[t].z_end) {
            pthread_join(threads[t], NULL);
            if (args[t].bytes_written > 0 && args[t].out) {
                memcpy(out_buf + total_out, args[t].out, args[t].bytes_written);
                total_out += args[t].bytes_written;
            }
        }
        free(args[t].out);
    }

    memcpy(out_buf + total_out, "\x1b[0m\x1b[?2026l", 12);
    total_out += 12;

    memcpy(prev_frame, frame, frame_bytes);
    return total_out;
}
