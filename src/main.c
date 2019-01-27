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

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stropts.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#include "load.h"
#include "proc-util.h"
#include "config.h"
#include "wlmenu.h"

static struct wlmenu wlmenu;
static struct item *list;
static size_t size;

static void *thr_load(void *arg)
{
    (void) arg;
    
    load(&list, &size);

    pthread_exit(NULL);
}

/* 
 * TODO: Command-line arguments
 *   --rows
 *   --fg, --bg, --border, --highlight-fg, --highlight-bg
 *   --font, --font-size
 */

int main(int argc, char *argv[])
{
    struct timespec begin, end;
    struct widget *widget;
    struct config conf;
    pthread_t thread;
    uint64_t elapsed;
    int err;

    (void) argc;
    (void) argv;

    (void) clock_gettime(CLOCK_MONOTONIC, &begin);

    err = pthread_create(&thread, NULL, &thr_load, NULL);
    if (err < 0)
        die("Failed to load runnable applications\n");

    config_init(&conf);

    wlmenu_init(&wlmenu, NULL);
    wlmenu_set_window_title(&wlmenu, "wlmenu");
    
    widget = wlmenu_widget(&wlmenu);
    widget_set_foreground(widget, conf.color_fg);
    widget_set_background(widget, conf.color_bg);
    widget_set_highlight_foreground(widget, conf.color_highlight_fg);
    widget_set_highlight_background(widget, conf.color_highlight_bg);
    widget_set_border(widget, conf.color_border);
    widget_set_font(widget, "/usr/share/fonts/TTF/Hack-Regular.ttf");
    widget_set_font_size(widget, 16.0);
    widget_set_max_rows(widget, conf.rows);

    wlmenu_show(&wlmenu);

    (void) pthread_join(thread, NULL);

    wlmenu_set_items(&wlmenu, list, size);

    (void) clock_gettime(CLOCK_MONOTONIC, &end);

    elapsed = 1000000000ul * (end.tv_sec - begin.tv_sec) 
            + (end.tv_nsec - begin.tv_nsec);

    printf("Init: %lu us / %lu ms\n", elapsed / 1000, elapsed / 1000000);

    printf("Entering dispatch mode\n");

    wlmenu_mainloop(&wlmenu);

    wlmenu_destroy(&wlmenu);
    config_destroy(&conf);
    fprintf(stdout, "Goodbye!\n");

    return EXIT_SUCCESS;
}
