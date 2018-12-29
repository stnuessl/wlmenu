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
        die("Out of memory\n");

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

    /* Merge the small sorted batches until everything is sorted */

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

static ssize_t do_cache_read(int fd, const char *path, struct item **list)
{
    char *iter = strdupa(path);
    size_t size = 0, n = 0, n_max;
    struct stat st;
    char *mem;
    int err;

    err = fstat(fd, &st);
    if (err < 0)
        return -errno;

    if (st.st_size < 0)
        return -EINVAL;

    /*
     * Check if the last modification to the cache was made after
     * changes to the directories specified in 'path'.
     */

    while (iter) {
        struct stat buf;
        char *str = strsep(&iter, ":");

        err = stat(str, &buf);
        if (err < 0)
            return -errno;

        if (buf.st_mtim.tv_sec < st.st_mtim.tv_sec)
            continue;

        if (buf.st_mtim.tv_sec > st.st_mtim.tv_sec)
            return -EINVAL;

        if (buf.st_mtim.tv_nsec > st.st_mtim.tv_nsec)
            return -EINVAL;
    }

    /* Read the whole file in one go */
    mem = malloc(st.st_size * sizeof(*mem));
    if (!mem)
        return -errno;

    do {
        ssize_t m = read(fd, mem + size, st.st_size - size);
        if (m < 0) {
            if (errno == EINTR)
                continue;

            return -errno;
        }

        size += m;
    } while (size < (size_t) st.st_size);

    /* Does 'path' match with the information stored in the cache */
    if (strcmp(path, strsep(&mem, "\n")) != 0)
        return -EINVAL;

    /* Retrieve the number of items stored in the cache */
    if (!mem || !isdigit(mem[0]))
        return -EINVAL;

    n_max = strtoul(mem, &mem, 10);

    *list = malloc(n_max * sizeof(**list));
    if (!*list)
        return -errno;

    /* Retrieve all items from the cache */
    while (mem) {
        char *line = strsep(&mem, "\n");
        size_t len = strlen(line);

        if (!len)
            continue;

        if (n >= n_max)
            return -EINVAL;

        (*list)[n].hits = 0;
        (*list)[n].name = line;

        ++n;
    }

    return n;
}

static ssize_t
cache_read(const char *cache, const char *path, struct item **list)
{
    int fd = open(cache, O_RDONLY);
    ssize_t size;

    if (fd < 0)
        return -errno;

    size = do_cache_read(fd, path, list);

    close(fd);

    return size;
}

static void
cache_write(const char *cache, const char *path, struct item *list, size_t size)
{
    FILE *file = fopen(cache, "w");
    if (!file)
        return;

    fprintf(file, "%s\n%lu\n\n", path, size);

    for (size_t i = 0; i < size; ++i)
        fprintf(file, "%s\n", list[i].name);

    fclose(file);
}

static size_t do_load(const char *path, const char *cache, struct item **list)
{
    size_t n = 0, n_max = 4096;
    char *iter;
    ssize_t size;

    size = cache_read(cache, path, list);
    if (size >= 0)
        return (size_t) size;

    *list = malloc(n_max * sizeof(**list));
    if (!list)
        die("Out of memory\n");

    iter = strdupa(path);
    while (iter) {
        char *token = strsep(&iter, ":");
        DIR *dir = opendir(token);
        int fd;

        if (!dir)
            continue;

        fd = dirfd(dir);

        while (1) {
            struct dirent *entry = readdir(dir);
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
                    die("Out of memory\n");
            }

            (*list)[n].hits = 0;
            (*list)[n].name = strdup(entry->d_name);

            if (!(*list)[n].name)
                die("Out of memory\n");

            ++n;
        }

        closedir(dir);
    }

    sort(*list, n);
    n = dedup(*list, n);

    cache_write(cache, path, *list, n);

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
