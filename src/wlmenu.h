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
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include <cairo.h>

#include "cairo-util.h"
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

    /* Font configuration related objects */
    FT_Library ft_lib;
    FT_Face ft_face;
    double font_size;

    /* Framebuffer configuration */
    void *mem;
    size_t size;

    int32_t width;
    int32_t height;
    int32_t stride;

    /* Cairo for rendering */
    cairo_t *cairo;

    /* Coordinates for rendering */
    int32_t input_rect_x;
    int32_t input_rect_y;
    int32_t input_rect_width;
    int32_t input_rect_height;
    int32_t input_glyph_x;
    int32_t input_glyph_y;

    cairo_font_extents_t font_extents;

    /* Text input */
    char input[32];
    size_t len;

    /* Keyboard configuration */
    int32_t rate;
    int32_t delay;
    xkb_keysym_t symbol;

    /* Color settings */
    struct color fg;
    struct color bg;
    struct color border;

    int epoll_fd;
    int timer_fd;

    uint8_t show : 1;
    uint8_t released : 1;
    uint8_t dirty : 1;
    uint8_t quit : 1;
};

void wlmenu_init(struct wlmenu *w, const char *display_name);

void wlmenu_destroy(struct wlmenu *w);

void wlmenu_set_font(struct wlmenu *w, const char *file);

void wlmenu_set_font_size(struct wlmenu *w, double font_size);

void wlmenu_set_window_title(struct wlmenu *w, const char *title);

void wlmenu_set_window_class(struct wlmenu *w, const char *name);

void wlmenu_set_foreground(struct wlmenu *w, uint32_t rgba);

void wlmenu_set_background(struct wlmenu *w, uint32_t rgba);

void wlmenu_set_border(struct wlmenu *w, uint32_t rgba);

void wlmenu_show(struct wlmenu *w);

void wlmenu_mainloop(struct wlmenu *w);

#endif /* WLMENU_H_ */
