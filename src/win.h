/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Steffen Nuessle
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WIN_H_
#define WIN_H_

#include <wayland-client.h>

#include "framebuffer.h"
#include "textbox.h"
#include "xkb.h"

struct win {
    struct xkb xkb;

    struct wl_display *display;
    struct wl_compositor *compositor;
    struct wl_subcompositor *subcompositor;
    struct wl_shell *shell;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_output *output;
    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;
    struct wl_buffer *buffer;

    struct framebuffer framebuffer;
    struct textbox textbox;

    uint32_t serial;

    int32_t rate;
    int32_t delay;
    xkb_keysym_t symbol;

    int fd_epoll;
    int fd_timer;

    uint8_t dirty : 1;
    uint8_t quit : 1;
};

void win_init(struct win *w, const char *display_name);

void win_destroy(struct win *w);

void win_set_title(struct win *w, const char *title);

void win_set_class(struct win *w, const char *name);

void win_show(struct win *w);

void win_mainloop(struct win *w);

#endif /* WIN_H_ */
