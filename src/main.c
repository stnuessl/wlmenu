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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "load.h"
#include "proc-util.h"
#include "win.h"

static struct win win;
static struct item *list;
static size_t size;

static void *thr_load(void *arg)
{
    (void) arg;

    size = load(&list);

    return NULL;
}

static void match(const char *str, struct item *list, size_t size)
{
    size_t len = strlen(str);
    if (!len)
        return;

    for (size_t i = 0; i < size; ++i) {
        int diff = (unsigned int) len - list[i].hits;

        if (abs(diff) == 1 && strstr(list[i].name, str))
            list[i].hits = (unsigned int) len;
    }
}

#if 0
static int make_directories(const char *path, mode_t mode)
{

    char *dup = strdupa(path);
    
    for (char *p = dup; *p != '\0'; ++p) {
        if (*p == '/') {
            int err;

            *p = '\0';

            err = mkdir(dup, mode);
            if (err < 0 && errno != EEXIST)
                return -errno;

            *p = '/';

            while (*p != 0 && *p++ == '/')
                ;
        }
    }

    return 0;
}
#endif

#if 0
void main_win_draw(void *arg)
{
    printf("Drawing!\n");

    struct window *win = arg;
    cairo_t *cr = window_cairo(win);
    
    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, .5);
    cairo_rectangle(cr, 0, 0, 200, 200);
    cairo_fill(cr);
//    cairo_paint(cr);
}
#endif

int main(int argc, char *argv[])
{
    pthread_t thread;
    int err;

    (void) argc;
    (void) argv;
    (void) &match;

    err = pthread_create(&thread, NULL, &thr_load, NULL);
    if (err < 0)
        die("Failed to load runnable applications\n");

    win_init(&win, NULL);
    win_set_title(&win, "wlmenu");
    win_show(&win);

    (void) pthread_join(thread, NULL);

    printf("Entering dispatch mode\n");

    win_mainloop(&win);

    win_destroy(&win);
    fprintf(stdout, "Goodbye!\n");

    return EXIT_SUCCESS;
}
