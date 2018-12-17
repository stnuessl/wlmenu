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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wayland-client.h>

#include "load.h"
#include "proc-util.h"

static struct wl_display *display = NULL;
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

int main(int argc, char *argv[])
{
    pthread_t thread;
    int err;

    err = pthread_create(&thread, NULL, &thr_load, NULL);
    if (err < 0)
        die("Failed to load applications\n");

    (void) pthread_join(thread, NULL);
#if 0
    match("f", list, size);
    match("fi", list, size);
    match("fir", list, size);
    match("fire", list, size);
    match("fir", list, size);
    match("fire", list, size); 
    match("fir", list, size);
    match("fi", list, size);
    match("f", list, size);
    match("", list, size);
    match("m", list, size);
    match("mu", list, size);
    match("mup", list, size);

    for (size_t i = 0; i < size; ++i) {
        if (list[i].hits == 3)
            printf("%s: %u\n", list[i].name, list[i].hits);
    }
#endif
#if 0
    for (size_t i = 0; i < size; ++i) {
        printf ("%s\n", list[i].name);
    }

#endif

#if 0    
    for (size_t i = 0; i < size; ++i) {
        printf("%s\n", list[i].name);
    }

    printf("\nsize = %lu\n\n", size);

// #if 0
    display = wl_display_connect(NULL);
    if (display == NULL) {
    	fprintf(stderr, "Can't connect to display\n");
	    exit(1);
    }
    printf("connected to display\n");

    wl_display_disconnect(display);
#endif
    fprintf(stdout, "Goodbye!\n");
}
