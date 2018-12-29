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

#include "win.h"

#include "proc-util.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

static void win_draw_background(struct win *w)
{
    cairo_t *cairo = framebuffer_cairo(&w->framebuffer);
    int32_t width = 400;
    int32_t height = 200;
    int32_t x = (framebuffer_width(&w->framebuffer) - width) / 2;
    int32_t y = (framebuffer_height(&w->framebuffer) - height) / 2;
    double val = 40.0 / 255.0;

    cairo_set_source_rgba(cairo, val, val, val, 1.0);
    cairo_rectangle(cairo, x, y, width, height);
    cairo_fill(cairo);

    cairo_set_source_rgba(cairo, 1.0, 0.0, 0.0, 1.0);
    cairo_set_line_width(cairo, 10.0);
    cairo_rectangle(cairo, x, y, width, height);
    cairo_stroke(cairo);

    wl_surface_damage_buffer(w->surface, x, y, width, height);
}

static void win_draw_text(struct win *w)
{
    cairo_t *cairo = framebuffer_cairo(&w->framebuffer);
    int32_t width = 400;
    int32_t height = 200;
    int32_t x = (framebuffer_width(&w->framebuffer) - width) / 2;
    int32_t y = (framebuffer_height(&w->framebuffer) - height) / 2;

    cairo_select_font_face(cairo, "Hack", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cairo, 16.0);
    cairo_set_source_rgba(cairo, 0.0, 255.0, 0.0, 1.0);
    cairo_move_to(cairo, x + 20.0, y + 20.0);
    cairo_show_text(cairo, textbox_str(&w->textbox));

    wl_surface_damage_buffer(w->surface, x, y, width, height);
}

static void win_draw(struct win *w)
{
    if (!w->buffer) {
        w->dirty = true;
        return;
    }

    win_draw_background(w);
    win_draw_text(w);

    wl_surface_attach(w->surface, w->buffer, 0, 0);
    wl_surface_commit(w->surface);

    w->buffer = NULL;
}

static void buffer_release(void *data, struct wl_buffer *buffer)
{
    struct win *w = data;

    w->buffer = buffer;
    printf("Buffer released!\n");

    if (w->dirty) {
        win_draw(w);
        w->dirty = false;
    }
}

static const struct wl_buffer_listener buffer_listener = {
    .release = &buffer_release,
};

static void
surface_enter(void *data, struct wl_surface *surface, struct wl_output *output)
{
    struct win *w = data;

    (void) w;
    (void) surface;
    (void) output;
}

static void
surface_leave(void *data, struct wl_surface *surface, struct wl_output *output)
{
    struct win *w = data;

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
    struct win *w = data;

    w->serial = serial;

    wl_shell_surface_pong(shell_surface, serial);
}

static void shell_surface_configure(void *data,
                                    struct wl_shell_surface *shell_surface,
                                    uint32_t edges,
                                    int32_t width,
                                    int32_t height)
{
    struct win *w = data;
    struct wl_shm_pool *pool;
    int32_t fb_width, fb_height;
    int fd, err;

    (void) shell_surface;
    (void) edges;

    fb_width = framebuffer_width(&w->framebuffer);
    fb_height = framebuffer_height(&w->framebuffer);

    if (fb_width == width && fb_height == height)
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

    w->buffer = wl_shm_pool_create_buffer(
        pool, 0, framebuffer_width(&w->framebuffer),
        framebuffer_height(&w->framebuffer),
        framebuffer_stride(&w->framebuffer), WL_SHM_FORMAT_ARGB8888);
    if (!w->buffer)
        die("Failed to create buffer for window surface\n");

    wl_buffer_add_listener(w->buffer, &buffer_listener, w);

    wl_shm_pool_destroy(pool);
    close(fd);

    win_draw(w);
}

static void shell_surface_popup_done(void *data,
                                     struct wl_shell_surface *shell_surface)
{
    struct win *w = data;

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
    struct win *w = data;

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
    struct win *w = data;

    (void) w;
    (void) output;
    (void) flags;
    (void) width;
    (void) height;
    (void) refresh;
}

static void output_done(void *data, struct wl_output *output)
{
    struct win *w = data;

    (void) w;
    (void) output;
}

static void output_scale(void *data, struct wl_output *output, int32_t factor)
{
    struct win *w = data;

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

static void win_dispatch_key_event(struct win *w, xkb_keysym_t symbol)
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

    win_draw(w);
}

static void keyboard_keymap(void *data,
                            struct wl_keyboard *keyboard,
                            uint32_t format,
                            int32_t fd,
                            uint32_t size)
{
    struct win *w = data;
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
    struct win *w = data;

    (void) w;
    (void) keyboard;
    (void) serial;
    (void) surface;
    (void) keys;
}

static void keyboard_leave(void *data,
                           struct wl_keyboard *keyboard,
                           uint32_t serial,
                           struct wl_surface *surface)
{
    struct win *w = data;

    (void) w;
    (void) keyboard;
    (void) serial;
    (void) surface;
}

static void keyboard_key(void *data,
                         struct wl_keyboard *keyboard,
                         uint32_t serial,
                         uint32_t time,
                         uint32_t key,
                         uint32_t state)
{
    struct win *w = data;
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

        win_dispatch_key_event(w, symbol);

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
    struct win *w = data;

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
    struct win *w = data;

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

static void registry_add(void *data,
                         struct wl_registry *registry,
                         uint32_t name,
                         const char *interface,
                         uint32_t version)
{
    struct win *w = data;
    void *item;

    (void) version;

    if (strcmp(interface, "wl_compositor") == 0) {
        item = wl_registry_bind(registry, name, &wl_compositor_interface, 4);

        if (item && w->compositor)
            wl_compositor_destroy(w->compositor);

        w->compositor = item;
        return;
    }

    if (strcmp(interface, "wl_subcompositor") == 0) {
        item = wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);

        if (item && w->subcompositor)
            wl_subcompositor_destroy(w->subcompositor);

        w->subcompositor = item;
        return;
    }

    if (strcmp(interface, "wl_shell") == 0) {
        item = wl_registry_bind(registry, name, &wl_shell_interface, 1);

        if (item && w->shell)
            wl_shell_destroy(w->shell);

        w->shell = item;
        return;
    }

    if (strcmp(interface, "wl_shm") == 0) {
        item = wl_registry_bind(registry, name, &wl_shm_interface, 1);

        if (item && w->shm)
            wl_shm_destroy(w->shm);

        w->shm = item;
        return;
    }

    if (strcmp(interface, "wl_seat") == 0) {
        struct wl_keyboard *keyboard;

        item = wl_registry_bind(registry, name, &wl_seat_interface, 4);

        if (item && w->seat)
            wl_seat_destroy(w->seat);

        w->seat = item;

        keyboard = wl_seat_get_keyboard(w->seat);
        wl_keyboard_add_listener(keyboard, &keyboard_listener, w);
        return;
    }

    if (strcmp(interface, "wl_output") == 0) {
        item = wl_registry_bind(registry, name, &wl_output_interface, 1);

        if (item && w->output)
            wl_output_destroy(w->output);

        w->output = item;
        return;
    }
}

static void
registry_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    struct win *w = data;

    (void) w;
    (void) registry;
    (void) name;
}

static const struct wl_registry_listener registry_listener = {
    .global = &registry_add,
    .global_remove = &registry_remove,
};

static void win_repeat_key(struct win *w)
{
    uint64_t val;
    ssize_t size;

    size = read(w->fd_timer, &val, sizeof(val));
    if (size != sizeof(val))
        return;

    win_dispatch_key_event(w, w->symbol);
}

static void win_dispatch_messages(struct win *w)
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

struct win_event {
    void (*run)(struct win *w);
};

static struct win_event wl_display_event = {.run = &win_dispatch_messages};
static struct win_event key_repeat_event = {.run = &win_repeat_key};

static void win_add_epoll_event(struct win *w, int fd, struct win_event *event)
{
    struct epoll_event ev;
    int err;

    ev.events = EPOLLIN;
    ev.data.ptr = event;

    err = epoll_ctl(w->fd_epoll, EPOLL_CTL_ADD, fd, &ev);
    if (err < 0)
        die_error(errno, "epoll_ctl()");
}

void win_init(struct win *w, const char *display_name)
{
    struct wl_registry *registry;

    xkb_init(&w->xkb);

    w->display = wl_display_connect(display_name);
    if (!w->display)
        die_error(errno, "Failed to connect to display manager");

    registry = wl_display_get_registry(w->display);
    if (!registry)
        die("Failed to create client registry for communication\n");

    wl_registry_add_listener(registry, &registry_listener, w);

    wl_display_roundtrip(w->display);

    if (!w->compositor)
        die("Failed to initialize compositor object\n");

    if (!w->subcompositor)
        die("Failed to initialize subcompositor object\n");

    if (!w->shell)
        die("Failed to initialize shell object\n");

    if (!w->shm)
        die("Failed to initialize shared memory object\n");

    if (!w->seat)
        die("Failed to initialize seat object\n");

    if (!w->output)
        die("Failed to retrieve output object\n");

    w->surface = wl_compositor_create_surface(w->compositor);
    if (!w->surface)
        die("Failed to create window surface\n");

    w->shell_surface = wl_shell_get_shell_surface(w->shell, w->surface);
    if (!w->shell_surface)
        die("Failed to create application window\n");

    wl_surface_add_listener(w->surface, &surface_listener, w);
    wl_shell_surface_add_listener(w->shell_surface, &shell_surface_listener, w);

    w->buffer = NULL;

    framebuffer_init(&w->framebuffer);
    textbox_init(&w->textbox);

    w->fd_epoll = epoll_create1(EPOLL_CLOEXEC);
    if (w->fd_epoll < 0)
        die_error(errno, "epoll_create1()");

    w->fd_timer = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (w->fd_timer < 0)
        die_error(errno, "timerfd_create()");

    win_add_epoll_event(w, w->fd_timer, &key_repeat_event);
    win_add_epoll_event(w, wl_display_get_fd(w->display), &wl_display_event);

    w->dirty = false;
    w->quit = false;
}

void win_destroy(struct win *w)
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
    wl_display_disconnect(w->display);

    xkb_destroy(&w->xkb);
}

void win_set_title(struct win *w, const char *title)
{
    wl_shell_surface_set_title(w->shell_surface, title);
}

void win_set_class(struct win *w, const char *name)
{
    wl_shell_surface_set_class(w->shell_surface, name);
}

void win_show(struct win *w)
{
    wl_shell_surface_set_maximized(w->shell_surface, NULL);
}

void win_mainloop(struct win *w)
{
    struct epoll_event events[2];

    while (!w->quit) {
        wl_display_flush(w->display);

        int n = epoll_wait(w->fd_epoll, events, ARRAY_SIZE(events), -1);
        if (n < 0 && errno != EINTR)
            die_error(errno, "epoll_wait()");

        while (n--)
            ((struct win_event *) events[n].data.ptr)->run(w);
    }
}
