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
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <time.h>
#include <unistd.h>

#include "util/die.h"

#include "config.h"
#include "load.h"
#include "wlmenu.h"

#define WLMENU_MAJOR_VERSION "1"
#define WLMENU_MINOR_VERSION "0"
#define WLMENU_PATCH_VERSION "0"

#define WLMENU_VERSION                                                         \
    (WLMENU_MAJOR_VERSION "." WLMENU_MINOR_VERSION "." WLMENU_PATCH_VERSION)

static struct wlmenu wlmenu;
static struct item *list;
static size_t size;

static void help(void)
{
    fprintf(stdout,
            "Usage: wlmenu [options]\n"
            "Options:\n"
            "--config, -c  [arg]  Use [arg] as the configuration file\n"
            "                     instead of \"~/.config/wlmenu/config\".\n"
            "--help, -h           Show this help message and exit.\n"
            "--version, -v        Print the version information and exit.\n");
}

static void *thr_load(void *arg)
{
    (void) arg;

    load(&list, &size);

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    struct timespec begin, end;
    struct config conf;
    char *path;
    pthread_t thread;
    uint64_t elapsed;
    int err;

    path = getenv("HOME");
    if (path) {
        size_t len1 = strlen(path);
        size_t len2 = sizeof("/.config/wlmenu/config");
        char *s = alloca(len1 + len2);

        memcpy(s, path, len1);
        memcpy(s + len1, "/.config/wlmenu/config", len2);

        path = s;
    }

    for (int i = 1; i < argc; ++i) {

        if (!strcmp(argv[i], "--config") || !strcmp(argv[i], "-c")) {
            if (++i >= argc)
                die("missing argument for option \"%s\"\n", argv[i - 1]);

            path = argv[i];
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            help();
            exit(EXIT_SUCCESS);
        } else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v")) {
            fprintf(stdout, "wlmenu: v%s\n", WLMENU_VERSION);
            exit(EXIT_SUCCESS);
        } else {
            fprintf(stderr, "wlmenu: unknown argument \"%s\"\n", argv[i]);
        }
    }

    (void) clock_gettime(CLOCK_MONOTONIC, &begin);

    err = pthread_create(&thread, NULL, &thr_load, NULL);
    if (err < 0)
        die("Failed to load runnable applications\n");

    config_init(&conf);
    config_load(&conf, path);

    wlmenu_init(&wlmenu, NULL);
    wlmenu_set_window_title(&wlmenu, "wlmenu");
    wlmenu_set_config(&wlmenu, &conf);

    wlmenu_show(&wlmenu);

    (void) pthread_join(thread, NULL);

    wlmenu_set_items(&wlmenu, list, size);

    (void) clock_gettime(CLOCK_MONOTONIC, &end);

    elapsed = 1000000000ul * (end.tv_sec - begin.tv_sec) +
              (end.tv_nsec - begin.tv_nsec);

    printf("Init: %lu us / %lu ms\n", elapsed / 1000, elapsed / 1000000);

    printf("Entering dispatch mode\n");

    wlmenu_run(&wlmenu);

    wlmenu_destroy(&wlmenu);
    config_destroy(&conf);
    fprintf(stdout, "Goodbye!\n");

    return EXIT_SUCCESS;
}
