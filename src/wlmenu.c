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

#include <errno.h>
#include <linux/memfd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include <cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "wlmenu.h"

#include "proc-util.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

static void wlmenu_draw_background(struct wlmenu *w)
{
    cairo_t *cairo = framebuffer_cairo(&w->framebuffer);

    cairo_set_source_rgba(cairo, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cairo);
}

static void wlmenu_draw_input_textbox(struct wlmenu *w)
{
    cairo_t *cairo = framebuffer_cairo(&w->framebuffer);
    int32_t max_width = framebuffer_width(&w->framebuffer);
    int32_t max_height = framebuffer_height(&w->framebuffer);

#if 1
    FT_Library ft_lib;
    FT_Face ft_face;
    cairo_font_face_t *font;
    cairo_scaled_font_t *scaled_font;
    cairo_glyph_t *glyphs = NULL;
    int n_glyphs = 0;
    cairo_status_t status;
    size_t size = textbox_size(&w->textbox);
    cairo_text_extents_t extents;
    cairo_font_extents_t font_extents;
    int err;

    err = FT_Init_FreeType(&ft_lib);
    if (err != 0)
        die("FT_Init_FreeType()\n");

    err = FT_New_Face(ft_lib, "/usr/share/fonts/TTF/Hack-Regular.ttf", 0,
                      &ft_face);
    if (err != 0)
        die("FT_New_Face()\n");

    font = cairo_ft_font_face_create_for_ft_face(ft_face, FT_LOAD_DEFAULT);
    if (cairo_font_face_status(font) != CAIRO_STATUS_SUCCESS)

        cairo_set_font_face(cairo, font);
    cairo_set_font_size(cairo, 16.0);

    scaled_font = cairo_get_scaled_font(cairo);
    if (cairo_scaled_font_status(scaled_font) != CAIRO_STATUS_SUCCESS)
        die("cairo_scaled_font_create()\n");

    cairo_scaled_font_extents(scaled_font, &font_extents);

    int32_t width = max_width / 3;
    int32_t height = font_extents.height * 2 - font_extents.height / 2;
    int32_t x = (max_width - width) / 2;
    int32_t y = (max_height - height) / 2;

    cairo_util_set_source(cairo, &w->bg);
    cairo_rectangle(cairo, x, y, width, height);
    cairo_fill_preserve(cairo);

    cairo_util_set_source(cairo, &w->border);
    cairo_set_line_width(cairo, 1.0);
    cairo_stroke(cairo);

    status = cairo_scaled_font_text_to_glyphs(
        scaled_font, x + font_extents.max_x_advance,
        y + (height + font_extents.ascent - font_extents.descent) / 2,
        textbox_str(&w->textbox), size, &glyphs, &n_glyphs, NULL, NULL, NULL);
    if (status != CAIRO_STATUS_SUCCESS)
        die("text_to_glyphs()\n");

    cairo_util_set_source(cairo, &w->fg);
    cairo_show_glyphs(cairo, glyphs, n_glyphs);

    cairo_glyph_free(glyphs);

#if 0
    cairo_font_face_destroy(font);
    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_lib);
#endif
#else
    cairo_select_font_face(cairo, "Hack", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cairo, 16.0);
    cairo_set_source_rgba(cairo, 0.0, 255.0, 0.0, 1.0);
    cairo_move_to(cairo, x + 20.0, y + 20.0);
    cairo_show_text(cairo, textbox_str(&w->textbox));
#endif

    wl_surface_damage_buffer(w->surface, 0, 0, max_width, max_height);
    //    wl_surface_damage_buffer(w->surface, x, y, width, height);
}

static void wlmenu_draw(struct wlmenu *w)
{
    if (!w->released) {
        w->dirty = true;
        return;
    }

    wlmenu_draw_background(w);
    wlmenu_draw_input_textbox(w);

    wl_surface_attach(w->surface, w->buffer, 0, 0);
    wl_surface_commit(w->surface);

    w->released = false;
}

static void buffer_release(void *data, struct wl_buffer *buffer)
{
    struct wlmenu *w = data;

    (void) buffer;

    w->released = true;

    if (w->dirty) {
        wlmenu_draw(w);
        w->dirty = false;
    }
}

static const struct wl_buffer_listener buffer_listener = {
    .release = &buffer_release,
};

static void
surface_enter(void *data, struct wl_surface *surface, struct wl_output *output)
{
    struct wlmenu *w = data;

    (void) w;
    (void) surface;
    (void) output;
}

static void
surface_leave(void *data, struct wl_surface *surface, struct wl_output *output)
{
    struct wlmenu *w = data;

    (void) w;
    (void) surface;
    (void) output;
}

static const struct wl_surface_listener surface_listener = {
    .enter = &surface_enter,
    .leave = &surface_leave,
};

static void shell_surface_ping(void *data,
                               struct wl_shell_surface *shell_surface,
                               uint32_t serial)
{
    struct wlmenu *w = data;

    w->serial = serial;

    wl_shell_surface_pong(shell_surface, serial);
}

static void shell_surface_configure(void *data,
                                    struct wl_shell_surface *shell_surface,
                                    uint32_t edges,
                                    int32_t width,
                                    int32_t height)
{
    struct wlmenu *w = data;
    struct wl_shm_pool *pool;
    int32_t current_width, current_height;
    int fd, err;

    (void) edges;

    if (w->shell_surface != shell_surface)
        die("shell_surface_configure(): invalid shell surface\n");

    current_width = framebuffer_width(&w->framebuffer);
    current_height = framebuffer_height(&w->framebuffer);

    if (current_width == width && current_height == height)
        return;

    if (width < 0 || height < 0)
        die("Invalid window configuration parameters\n");

    if (w->buffer)
        wl_buffer_destroy(w->buffer);

    fd = memfd_create("wlmenu-shm", MFD_CLOEXEC);
    if (fd < 0)
        die_error(errno, "Failed to create shared memory region");

    err = framebuffer_configure(&w->framebuffer, fd, width, height,
                                CAIRO_FORMAT_ARGB32);
    if (err < 0)
        die_error(-err, "Failed to configure framebuffer");

    pool = wl_shm_create_pool(w->shm, fd, framebuffer_size(&w->framebuffer));
    if (!pool)
        die("Failed to share framebuffer with display manager\n");

    /* clang-format off */
    w->buffer = wl_shm_pool_create_buffer(pool,
                                          0,
                                          framebuffer_width(&w->framebuffer),
                                          framebuffer_height(&w->framebuffer),
                                          framebuffer_stride(&w->framebuffer),
                                          WL_SHM_FORMAT_ARGB8888);
    /* clang-format on */
    if (!w->buffer)
        die("Failed to create buffer for window surface\n");

    wl_buffer_add_listener(w->buffer, &buffer_listener, w);
    w->released = true;

    wl_shm_pool_destroy(pool);
    close(fd);

    wlmenu_draw(w);
}

static void shell_surface_popup_done(void *data,
                                     struct wl_shell_surface *shell_surface)
{
    struct wlmenu *w = data;

    (void) w;
    (void) shell_surface;
}

static const struct wl_shell_surface_listener shell_surface_listener = {
    .ping = &shell_surface_ping,
    .configure = &shell_surface_configure,
    .popup_done = &shell_surface_popup_done,
};

static void output_geometry(void *data,
                            struct wl_output *output,
                            int32_t x,
                            int32_t y,
                            int32_t physical_width,
                            int32_t physical_height,
                            int32_t subpixel,
                            const char *make,
                            const char *model,
                            int32_t transform)
{
    struct wlmenu *w = data;

    (void) w;
    (void) output;
    (void) x;
    (void) y;
    (void) physical_width;
    (void) physical_height;
    (void) subpixel;
    (void) make;
    (void) model;
    (void) transform;
}

static void output_mode(void *data,
                        struct wl_output *output,
                        uint32_t flags,
                        int32_t width,
                        int32_t height,
                        int32_t refresh)
{
    struct wlmenu *w = data;

    (void) w;
    (void) output;
    (void) flags;
    (void) width;
    (void) height;
    (void) refresh;
}

static void output_done(void *data, struct wl_output *output)
{
    struct wlmenu *w = data;

    (void) w;
    (void) output;
}

static void output_scale(void *data, struct wl_output *output, int32_t factor)
{
    struct wlmenu *w = data;

    (void) w;
    (void) output;
    (void) factor;
}

const struct wl_output_listener output_listener = {
    .geometry = &output_geometry,
    .mode = &output_mode,
    .done = &output_done,
    .scale = &output_scale,
};

static void wlmenu_dispatch_key_event(struct wlmenu *w, xkb_keysym_t symbol)
{
    switch (symbol) {
    case XKB_KEY_Return:
        w->quit = true;
        break;
    case XKB_KEY_BackSpace:
        textbox_remove(&w->textbox);
        break;
    case XKB_KEY_Tab:
        break;
    case XKB_KEY_Up:
        break;
    case XKB_KEY_Down:
        break;
    case XKB_KEY_NoSymbol:
        break;
    default:
        textbox_insert(&w->textbox, (int) symbol);
        break;
    }

    wlmenu_draw(w);
}

static void keyboard_keymap(void *data,
                            struct wl_keyboard *keyboard,
                            uint32_t format,
                            int32_t fd,
                            uint32_t size)
{
    struct wlmenu *w = data;
    char *mem;
    bool ok;

    (void) keyboard;

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
        die("Received invalid keymap format\n");

    mem = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED)
        die("Failed to integrate received keymap information\n");

    ok = xkb_set_keymap(&w->xkb, mem);
    if (!ok)
        die("Failed to initialize received keymap\n");

    munmap(mem, size);
    close(fd);
}

static void keyboard_enter(void *data,
                           struct wl_keyboard *keyboard,
                           uint32_t serial,
                           struct wl_surface *surface,
                           struct wl_array *keys)
{
    struct wlmenu *w = data;

    (void) keyboard;
    (void) surface;
    (void) keys;

    w->serial = serial;
}

static void keyboard_leave(void *data,
                           struct wl_keyboard *keyboard,
                           uint32_t serial,
                           struct wl_surface *surface)
{
    struct wlmenu *w = data;

    (void) keyboard;
    (void) surface;

    w->serial = serial;
}

static void keyboard_key(void *data,
                         struct wl_keyboard *keyboard,
                         uint32_t serial,
                         uint32_t time,
                         uint32_t key,
                         uint32_t state)
{
    struct wlmenu *w = data;
    struct itimerspec its;
    xkb_keysym_t symbol;
    int err;

    (void) keyboard;
    (void) serial;
    (void) time;

    if (serial <= w->serial)
        return;

    w->serial = serial;

    if (!xkb_keymap_ok(&w->xkb))
        return;

    switch (state) {
    case WL_KEYBOARD_KEY_STATE_PRESSED:
        symbol = xkb_get_sym(&w->xkb, key);

        wlmenu_dispatch_key_event(w, symbol);

        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = w->rate;
        its.it_value.tv_sec = 0;
        its.it_value.tv_nsec = w->delay;

        err = timerfd_settime(w->fd_timer, 0, &its, NULL);
        if (err < 0)
            die_error(errno, "timerfd_settime(): failed to start timer\n");

        w->symbol = symbol;
        break;
    case WL_KEYBOARD_KEY_STATE_RELEASED:

        its.it_value.tv_sec = 0;
        its.it_value.tv_nsec = 0;
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 0;

        err = timerfd_settime(w->fd_timer, 0, &its, NULL);
        if (err < 0)
            die_error(errno, "timerfd_settime(): failed to cancel timer\n");

        break;
    default:
        break;
    }
}

static void keyboard_modifiers(void *data,
                               struct wl_keyboard *keyboard,
                               uint32_t serial,
                               uint32_t mods_depressed,
                               uint32_t mods_latched,
                               uint32_t mods_locked,
                               uint32_t group)
{
    struct wlmenu *w = data;

    (void) keyboard;
    (void) serial;

    if (!xkb_keymap_ok(&w->xkb))
        return;

    xkb_state_update(&w->xkb, mods_depressed, mods_latched, mods_locked, group);
}

static void keyboard_repeat_info(void *data,
                                 struct wl_keyboard *keyboard,
                                 int32_t rate,
                                 int32_t delay)
{
    struct wlmenu *w = data;

    (void) keyboard;

    /* Convert to itimerspec units */
    w->rate = 1000000000 / rate;
    w->delay = 1000000 * delay;
}

const struct wl_keyboard_listener keyboard_listener = {
    .keymap = &keyboard_keymap,
    .enter = &keyboard_enter,
    .leave = &keyboard_leave,
    .key = &keyboard_key,
    .modifiers = &keyboard_modifiers,
    .repeat_info = &keyboard_repeat_info,
};

static void
wlmenu_bind_compositor(struct wlmenu *w, uint32_t name, uint32_t version)
{
    const struct wl_interface *interface = &wl_compositor_interface;

    w->compositor = wl_registry_bind(w->registry, name, interface, version);
    if (!w->compositor)
        die("Failed to bind to compositor interface\n");
}

static void
wlmenu_bind_subcompositor(struct wlmenu *w, uint32_t name, uint32_t version)
{
    const struct wl_interface *interface = &wl_subcompositor_interface;

    w->subcompositor = wl_registry_bind(w->registry, name, interface, version);
    if (!w->subcompositor)
        die("Failed to bind to subcompositor interface\n");
}

static void wlmenu_bind_shell(struct wlmenu *w, uint32_t name, uint32_t version)
{
    const struct wl_interface *interface = &wl_shell_interface;

    w->shell = wl_registry_bind(w->registry, name, interface, version);
    if (!w->shell)
        die("Failed to bind to shell interface\n");
}

static void wlmenu_bind_shm(struct wlmenu *w, uint32_t name, uint32_t version)
{
    const struct wl_interface *interface = &wl_shm_interface;

    w->shm = wl_registry_bind(w->registry, name, interface, version);
    if (!w->shm)
        die("Failed to bind to shared memory interface\n");
}

static void wlmenu_bind_seat(struct wlmenu *w, uint32_t name, uint32_t version)
{
    const struct wl_interface *interface = &wl_seat_interface;

    /* We can make this hot-pluggable */
    if (w->keyboard)
        wl_keyboard_destroy(w->keyboard);

    if (w->seat)
        wl_seat_destroy(w->seat);

    w->seat = wl_registry_bind(w->registry, name, interface, version);
    if (!w->seat)
        die("Failed to bind to seat interface\n");

    w->keyboard = wl_seat_get_keyboard(w->seat);
    if (!w->keyboard)
        die("Failed to bind to keyboard interface\n");

    wl_keyboard_add_listener(w->keyboard, &keyboard_listener, w);
}

static void
wlmenu_bind_output(struct wlmenu *w, uint32_t name, uint32_t version)
{
    const struct wl_interface *interface = &wl_output_interface;

    /* We can make this hot-pluggable */
    if (w->output)
        wl_output_destroy(w->output);

    w->output = wl_registry_bind(w->registry, name, interface, version);
    if (!w->output)
        die("Failed to bind to output interface\n");
}

static void registry_add(void *data,
                         struct wl_registry *registry,
                         uint32_t name,
                         const char *interface,
                         uint32_t version)
{
    struct wlmenu *w = data;

    (void) registry;

    if (strcmp(interface, "wl_compositor") == 0) {
        wlmenu_bind_compositor(w, name, version);
        return;
    }

    if (strcmp(interface, "wl_subcompositor") == 0) {
        wlmenu_bind_subcompositor(w, name, version);
        return;
    }
    
    if (strcmp(interface, "wl_shell") == 0) {
        wlmenu_bind_shell(w, name, version);
        return;
    }
    
    if (strcmp(interface, "wl_shm") == 0) {
        wlmenu_bind_shm(w, name, version);
        return;
    }

    if (strcmp(interface, "wl_seat") == 0) {
        wlmenu_bind_seat(w, name, version);
        return;
    }

    if (strcmp(interface, "wl_output") == 0) {
        wlmenu_bind_output(w, name, version);
        return;
    }
}

static void
registry_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    struct wlmenu *w = data;

    (void) w;
    (void) registry;
    (void) name;
}

static const struct wl_registry_listener registry_listener = {
    .global = &registry_add,
    .global_remove = &registry_remove,
};

static void wlmenu_repeat_key(struct wlmenu *w)
{
    uint64_t val;
    ssize_t size;

    size = read(w->fd_timer, &val, sizeof(val));
    if (size != sizeof(val))
        return;

    wlmenu_dispatch_key_event(w, w->symbol);
}

static void wlmenu_dispatch_messages(struct wlmenu *w)
{
    int err;

    err = wl_display_prepare_read(w->display);
    if (err < 0)
        die_error(errno, "Failed to prepare to read from display connection");

    err = wl_display_read_events(w->display);
    if (err < 0)
        die_error(errno, "Failed to read from display connection");

    err = wl_display_dispatch_pending(w->display);
    if (err < 0)
        die_error(errno, "Failed to dispatch messages from display connection");
}

struct wlmenu_event {
    void (*run)(struct wlmenu *w);
};

/* clang-format off */
static struct wlmenu_event wl_display_event = {
    .run = &wlmenu_dispatch_messages
};

static struct wlmenu_event key_repeat_event = {
    .run = &wlmenu_repeat_key
};
/* clang-format on */

static void
wlmenu_add_epoll_event(struct wlmenu *w, int fd, struct wlmenu_event *event)
{
    struct epoll_event ev;
    int err;

    ev.events = EPOLLIN;
    ev.data.ptr = event;

    err = epoll_ctl(w->fd_epoll, EPOLL_CTL_ADD, fd, &ev);
    if (err < 0)
        die_error(errno, "epoll_ctl()");
}

void wlmenu_init(struct wlmenu *w, const char *display_name)
{
    memset(w, 0, sizeof(*w));

    xkb_init(&w->xkb);

    w->display = wl_display_connect(display_name);
    if (!w->display)
        die_error(errno, "Failed to connect to display manager");

    w->registry = wl_display_get_registry(w->display);
    if (!w->registry)
        die("Failed to create registry for binding to server interfaces\n");

    wl_registry_add_listener(w->registry, &registry_listener, w);

    wl_display_roundtrip(w->display);

    if (!w->compositor)
        die_error(EPROTO, "Didn't receive compositor interface");

    if (!w->subcompositor)
        die_error(EPROTO, "Didn't receive subcompositor interface");

    if (!w->shell)
        die_error(EPROTO, "Didn't receive shell interface");

    if (!w->shm)
        die_error(EPROTO, "Didn't receive shared memory interface");

    if (!w->seat)
        die_error(EPROTO, "Didn't receive seat interface");

    if (!w->output)
        die_error(EPROTO, "Didn't receive output interface");

    w->surface = wl_compositor_create_surface(w->compositor);
    if (!w->surface)
        die("Failed to create application surface\n");

    w->shell_surface = wl_shell_get_shell_surface(w->shell, w->surface);
    if (!w->shell_surface)
        die("Failed to create application window\n");
    
    wl_surface_add_listener(w->surface, &surface_listener, w);
    wl_shell_surface_add_listener(w->shell_surface, &shell_surface_listener, w);

    framebuffer_init(&w->framebuffer);
    textbox_init(&w->textbox);

    w->fd_epoll = epoll_create1(EPOLL_CLOEXEC);
    if (w->fd_epoll < 0)
        die_error(errno, "epoll_create1()");

    w->fd_timer = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (w->fd_timer < 0)
        die_error(errno, "timerfd_create()");

    wlmenu_add_epoll_event(w, w->fd_timer, &key_repeat_event);
    wlmenu_add_epoll_event(w, wl_display_get_fd(w->display), &wl_display_event);
}

void wlmenu_destroy(struct wlmenu *w)
{
    close(w->fd_timer);
    close(w->fd_epoll);

    textbox_destroy(&w->textbox);
    framebuffer_destroy(&w->framebuffer);

    if (w->buffer)
        wl_buffer_destroy(w->buffer);

    wl_shell_surface_destroy(w->shell_surface);
    wl_surface_destroy(w->surface);
    wl_output_destroy(w->output);
    wl_seat_destroy(w->seat);
    wl_shm_destroy(w->shm);
    wl_shell_destroy(w->shell);
    wl_subcompositor_destroy(w->subcompositor);
    wl_compositor_destroy(w->compositor);
    wl_registry_destroy(w->registry);
    wl_display_disconnect(w->display);

    xkb_destroy(&w->xkb);
}

void wlmenu_set_window_title(struct wlmenu *w, const char *title)
{
    wl_shell_surface_set_title(w->shell_surface, title);
}

void wlmenu_set_window_class(struct wlmenu *w, const char *name)
{
    wl_shell_surface_set_class(w->shell_surface, name);
}

void wlmenu_set_foreground(struct wlmenu *w, uint32_t rgba)
{
    cairo_util_make_color_u32(rgba, &w->fg);
}

void wlmenu_set_background(struct wlmenu *w, uint32_t rgba)
{
    cairo_util_make_color_u32(rgba, &w->bg);
}

void wlmenu_set_border(struct wlmenu *w, uint32_t rgba)
{
    cairo_util_make_color_u32(rgba, &w->border);
}

void wlmenu_show(struct wlmenu *w)
{
    wl_shell_surface_set_maximized(w->shell_surface, NULL);
}

void wlmenu_mainloop(struct wlmenu *w)
{
    struct epoll_event events[2];

    while (!w->quit) {
        wl_display_flush(w->display);

        int n = epoll_wait(w->fd_epoll, events, ARRAY_SIZE(events), -1);
        if (n < 0 && errno != EINTR)
            die_error(errno, "epoll_wait()");

        while (n--)
            ((struct wlmenu_event *) events[n].data.ptr)->run(w);
    }
}
