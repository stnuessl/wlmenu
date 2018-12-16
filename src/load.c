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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "proc-util.h"

#include "load.h"

static void merge(struct item *dst,
                  struct item *src,
                  size_t i,
                  size_t e1,
                  size_t j,
                  size_t e2)
{
    while (i < e1 && j < e2) {
        if (strverscmp(src[i].name, src[j].name) <= 0)
            dst++->name = src[i++].name;
        else
            dst++->name = src[j++].name;
    }

    while (i < e1)
        dst++->name = src[i++].name;

    while (j < e2)
        dst++->name = src[j++].name;
}

static void sort(struct item *list, size_t size)
{
    struct item *buf = malloc(size * sizeof(*buf));
    struct item item = {NULL, 0};
    size_t n = 16;

    if (!buf)
        die("Out of memory!\n");

    /* Sort small batches of size 'n' with insertion sort */
    for (size_t i = 0; i < size; i += n) {
        size_t end = i + n;

        if (end >= size)
            end = size;

        for (size_t j = i + 1; j < end; ++j) {
            size_t k = j;

            item.name = list[j].name;

            while (k > i && strverscmp(list[k - 1].name, item.name) > 0) {
                list[k].name = list[k - 1].name;
                --k;
            }

            list[k].name = item.name;
        }
    }

    /* Merge the small sorted batches */

    while (n < size) {
        struct item *dst = buf;
        struct item *src = list;

        for (size_t i = 0; i < size; i += (n + n)) {
            size_t j = i + n;
            size_t k = j + n;

            if (j >= size)
                j = size;

            if (k >= size)
                k = size;

            merge(dst, src, i, j, j, k);
            dst += k - i;
        }

        n *= 2;

        dst = list;
        src = buf;

        for (size_t i = 0; i < size; i += (n + n)) {
            size_t j = i + n;
            size_t k = j + n;

            if (j >= size)
                j = size;

            if (k >= size)
                k = size;

            merge(dst, src, i, j, j, k);
            dst += k - i;
        }

        n *= 2;
    }
}

static size_t dedup(struct item *list, size_t size)
{
    size_t i = 0;

    for (size_t j = 1; j < size; ++j) {
        if (strcmp(list[i].name, list[j].name) != 0)
            list[++i].name = list[j].name;
    }

    return i;
}

static bool cache_ok(const char *cache, char *path)
{
    struct stat st;
    struct timespec ts;
    int err;

    /*
     * Check if the last modification to the cache was made after
     * changes to the directories specified in 'path'.
     */

    err = stat(cache, &st);
    if (err < 0)
        return false;

    ts.tv_sec = st.st_mtim.tv_sec;
    ts.tv_nsec = st.st_mtim.tv_nsec;

    while (path) {
        char *token = strsep(&path, ":");

        err = stat(token, &st);
        if (err < 0)
            return false;

        if (st.st_mtim.tv_sec > ts.tv_sec)
            return false;

        if (st.st_mtim.tv_sec == ts.tv_sec && st.st_mtim.tv_nsec > ts.tv_nsec)
            return false;
    }

    return true;
}

static size_t cache_read(const char *cache, struct item **list)
{
    size_t size = 0, n = 0, n_max = 4096;
    struct stat st;
    char *mem;
    int fd, err;

    fd = open(cache, O_RDONLY);
    if (fd < 0)
        die("Failed to open cache\n");

    err = fstat(fd, &st);
    if (err < 0 || st.st_size < 0)
        die("Cache corrupt!\n");

    mem = malloc(st.st_size * sizeof(*mem));
    if (!mem)
        die("Out of memory!\n");

    do {
        ssize_t m = read(fd, mem + size, st.st_size - size);
        if (m < 0) {
            if (errno == EINTR)
                continue;
            else
                die("Failed to read cache\n");
        }

        size += m;
    } while (size != (size_t) st.st_size);

    /*
     * Because this is a "one shot" application we do
     * not care about releasing any memory.
     */
    *list = malloc(n_max * sizeof(**list));
    if (!*list)
        die("Out of memory!\n");

    while (mem) {
        char *line = strsep(&mem, "\n");
        size_t len = strlen(line);

        if (len && (line[0] == '#' || line[0] == ';' || isspace(line[0])))
            continue;

        while (len && isspace(line[len]))
            line[len--] = '\0';

        if (!len)
            continue;

        if (n >= n_max) {
            n_max = n_max * 2 - n_max / 2;

            *list = realloc(*list, n_max * sizeof(**list));
            if (!*list)
                die("Out of memory!\n");
        }

        (*list)[n].hits = 0;
        (*list)[n].name = line;

        ++n;
    }

    close(fd);

    return n;
}

static void cache_write(const char *cache, struct item *list, size_t size)
{
    FILE *file = fopen(cache, "w");
    if (!file)
        return;

    fprintf(file, "#\n# Automatically generated by wlmenu\n#\n\n");

    for (size_t i = 0; i < size; ++i)
        fprintf(file, "%s\n", list[i].name);

    fclose(file);
}

static size_t do_load(char *path, char *cache, struct item **list)
{
    size_t n_max = 4096;
    size_t n = 0;

    if (cache_ok(cache, path))
        return cache_read(cache, list);

    *list = malloc(n_max * sizeof(**list));
    if (!list)
        die("Out of memory!\n");

    while (path) {
        char *token = strsep(&path, ":");
        DIR *directory = opendir(token);
        int fd;

        if (!directory)
            continue;

        fd = dirfd(directory);

        while (1) {
            struct dirent *entry = readdir(directory);
            struct stat st;
            int err;

            if (!entry)
                break;

            err = fstatat(fd, entry->d_name, &st, 0);
            if (err < 0)
                continue;

            if (!S_ISREG(st.st_mode) || !(st.st_mode & S_IXUSR))
                continue;

            if (n >= n_max) {
                n_max = n_max * 2 - n_max / 2;

                *list = realloc(*list, n_max * sizeof(**list));
                if (!*list)
                    die("Out of memory!\n");
            }

            (*list)[n].hits = 0;
            (*list)[n].name = strdup(entry->d_name);

            if (!(*list)[n].name)
                die("Out of memory!\n");

            ++n;
        }
    }

    sort(*list, n);
    n = dedup(*list, n);

    cache_write(cache, *list, n);

    return n;
}

size_t load(struct item **list)
{
    char *path, *cache;
    size_t n;
    char *env_path = getenv("PATH");
    char *env_home = getenv("HOME");

    if (!list)
        die("load(): Invalid argument\n");

    if (!env_path)
        die("Failed to retrieve ${PATH}\n");

    if (!env_home)
        die("Failed to retrieve ${HOME}\n");

    path = strdupa(env_path);

    n = strlen(env_home);
    cache = alloca(n + sizeof("/.cache/wlmenu/cache"));

    strcpy(cache, env_home);
    strcpy(cache + n, "/.cache/wlmenu/cache");

    return do_load(path, cache, list);
}
