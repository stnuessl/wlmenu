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

#ifndef WLMENU_H_
#define WLMENU_H_

#include <wayland-client.h>

#include "config.h"
#include "load.h"
#include "widget.h"
#include "xkb.h"

struct wlmenu {
    struct xkb xkb;

    /* Wayland globals */
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_subcompositor *subcompositor;
    struct wl_shell *shell;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_output *output;

    /* Window related wayland objects */
    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;
    struct wl_buffer *buffer;

    /* Wayland protocol freshness value */
    uint32_t serial;

    /* Framebuffer configuration */
    void *mem;
    size_t size;

    int32_t width;
    int32_t height;
    int32_t stride;

    struct widget widget;

    /* Runnable commands */
    struct item *items;
    size_t n;

    /* Keyboard configuration */
    int32_t rate;
    int32_t delay;
    xkb_keysym_t symbol;

    int epoll_fd;
    int timer_fd;

    uint8_t show : 1;
    uint8_t released : 1;
    uint8_t dirty : 1;
    uint8_t exec : 1;
};

void wlmenu_init(struct wlmenu *w, const char *display_name);

void wlmenu_destroy(struct wlmenu *w);

void wlmenu_set_window_title(struct wlmenu *w, const char *title);

void wlmenu_set_window_class(struct wlmenu *w, const char *name);

void wlmenu_set_exec(struct wlmenu *w, bool exec);

struct widget *wlmenu_widget(struct wlmenu *w);

void wlmenu_set_items(struct wlmenu *w, struct item *items, size_t size);

void wlmenu_set_config(struct wlmenu *w, const struct config *c);

void wlmenu_show(struct wlmenu *w);

void wlmenu_run(struct wlmenu *w);

#endif /* WLMENU_H_ */
