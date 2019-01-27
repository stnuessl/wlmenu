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
#include "xmalloc.h"
#include "xstring.h"

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

static void sort(struct item *items, size_t size)
{
    struct item *buf = xmalloc(size * sizeof(*buf));
    struct item item = { NULL, 0 };
    size_t n = 16;

    /* Sort small batches of size 'n' with insertion sort */
    for (size_t i = 0; i < size; i += n) {
        size_t end = i + n;

        if (end >= size)
            end = size;

        for (size_t j = i + 1; j < end; ++j) {
            size_t k = j;

            item.name = items[j].name;

            while (k > i && strverscmp(items[k - 1].name, item.name) > 0) {
                items[k].name = items[k - 1].name;
                --k;
            }

            items[k].name = item.name;
        }
    }

    /* Merge the small sorted batches until everything is sorted */

    while (n < size) {
        struct item *dst = buf;
        struct item *src = items;

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

        dst = items;
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

    free(buf);
}

static void dedup(struct item *items, size_t *size)
{
    size_t i = 0, n = *size;

    for (size_t j = 1; j < n; ++j) {
        if (strcmp(items[i].name, items[j].name) != 0)
            items[++i].name = items[j].name;
    }

    *size = i;
}

static int cache_read_data_str(int fd, const char *path, char **data)
{
    struct stat stat_cache;
    size_t n = 0;
    char *buf;
    int err;

    err = fstat(fd, &stat_cache);
    if (err < 0)
        return -errno;

    if (stat_cache.st_size < 0)
        return -EINVAL;

    if (!stat_cache.st_size) {
        *data = NULL;
        return 0;
    }

    /*
     * Check if the last modification to the cache was made after
     * the last modification to the directories specified in 'path'.
     */
    
    buf = strdupa(path);
    while (buf) {
        char *str = buf;
        struct stat st;
        int err;
        
        buf = strchr(buf, ':');
        if (buf)
            *buf++ = '\0';

        err = stat(str, &st);
        if (err < 0)
            return -errno;

        /* 
         * Is the cache definitly younger than the last modification to
         * directory 'str'?
         */
        if (st.st_mtim.tv_sec < stat_cache.st_mtim.tv_sec)
            continue;
        
        /* 
         * Is the cache definitly older than the last modification to
         * directory 'str'
         */
        if (st.st_mtim.tv_sec > stat_cache.st_mtim.tv_sec)
            return -EINVAL;
        
        /*
         * Both timestamps are about the same age.
         * Compare them on a finer granularity.
         */
        if (st.st_mtim.tv_nsec > stat_cache.st_mtim.tv_nsec)
            return -EINVAL;
    }

    /* Read in the whole file as a string */
    *data = xmalloc((size_t) stat_cache.st_size + 1);

    do {
        ssize_t m = read(fd, *data + n, (size_t) stat_cache.st_size - n);
        if (m < 0) {
            if (errno == EINTR)
                continue;

            return -errno;
        }

        n += m;
    } while (n < (size_t) stat_cache.st_size);

    (*data)[stat_cache.st_size] = '\0';

    return 0;
}

static int 
do_cache_read(int fd, const char *path, struct item **items, size_t *size)
{
    char *data, *str;
    size_t n = 0, n_max;
    int err;

    err = cache_read_data_str(fd, path, &data);
    if (err < 0)
        return err;

    if (!data)
        return -EINVAL;

    /* Does 'path' match with the information stored in the cache */
    str = data;
    data = strchr(data, '\n');

    if (!data)
        return -EINVAL;

    *data++ = '\0';

    if (strcmp(path, str) != 0)
        return -EINVAL;

    /* Retrieve the number of items stored in the cache */
    if (!isdigit(data[0]))
        return -EINVAL;

    n_max = strtoul(data, &data, 10);

    /* Retrieve all items which from the cache */
    *items = xmalloc(n_max * sizeof(**items));

    while (data) {
        char *str = data;
        data = strchr(data, '\n');

        if (data)
            *data++ = '\0';

        if (!strlen(str))
            continue;

        if (n >= n_max)
            return -EINVAL;

        (*items)[n].hits = 0;
        (*items)[n].name = str;

        ++n;
    }

    *size = n;

    return 0;
}

static int cache_read(const char *cache,
                      const char *path, 
                      struct item **items,
                      size_t *size)
{
    int fd, err;
    
    fd = open(cache, O_RDONLY);
    if (fd < 0)
        return -errno;

    err = do_cache_read(fd, path, items, size);

    close(fd);

    return err;
}

static void
cache_write(const char *cache, const char *path, struct item *items, size_t size)
{
    FILE *file = fopen(cache, "w");
    if (!file)
        return;

    fprintf(file, "%s\n%lu\n\n", path, size);

    for (size_t i = 0; i < size; ++i)
        fprintf(file, "%s\n", items[i].name);

    fclose(file);
}

static void do_path_load(const char *path, 
                         const char *cache, 
                         struct item **items,
                         size_t *size)
{
    size_t n = 0, n_max = 4096;
    char *iter;
    int err;

    /* Check if we can use previously cached data */
    err = cache_read(cache, path, items, size);
    if (!err)
        return;

    *items = xmalloc(n_max * sizeof(**items));

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

                *items = xrealloc(*items, n_max * sizeof(**items));
            }

            (*items)[n].hits = 0;
            (*items)[n].name = xstrdup(entry->d_name);

            ++n;
        }

        closedir(dir);
    }

    sort(*items, n);
    dedup(*items, &n);

    cache_write(cache, path, *items, n);

    *size = n;
}

static void load_from_stdin(struct item **items, size_t *size)
{
    char *data;
    size_t n = 0, n_max = 4096;

    data = xmalloc(n_max);

    while (1) {
        ssize_t m = read(STDIN_FILENO, data + n, n_max - n);
        if (m < 0) {
            if (errno == EINTR)
                continue;
            
            die_error(errno, "Failed to read data from stdin");
        }

        if (!m)
            break;

        n += m;

        if (n >= n_max) {
            n_max *= 2;

            data = xrealloc(data, n_max);
        }
    }
    
    if (!n) {
        free(data);
        *size = 0;

        return;
    }

    if (n >= n_max)
        data = xrealloc(data, n_max + 1);

    data[n] = '\0';

    /* Get a good initial value for the item allocation */
    n_max = n / 40;
    n = 0;

    *items = xmalloc(n_max * sizeof(**items));

    while (data) {
        char *p;
        char *str = data;
        data = strchr(data, '\n');

        if (data)
            *data++ = '\0';

        while (*str != '\0' && isspace(*str))
            ++str;

        p = str;

        while (*p != '\0' && !isspace(*p))
            ++p;

        *p = '\0';

        if (str == p)
            continue;

        if (n >= n_max) {
            n_max = n_max * 2 - n_max / 2;

            *items = xrealloc(*items, n_max * sizeof(**items));
        }

        (*items)[n].hits = 0;
        (*items)[n].name = str;

        ++n;
    }
    
    *size = n;
}

static void load_from_path(struct item **items, size_t *size)
{
    char *path, *cache;
    size_t n;
    char *env_path = getenv("PATH");
    char *env_home = getenv("HOME");

    if (!items)
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

    do_path_load(path, cache, items, size);
}

void load(struct item **items, size_t *size)
{
    struct stat st;
    int err;

    err = fstat(STDIN_FILENO, &st);
    if (err < 0)
        die_error(errno, "Failed to retrieve status of stdin");

    if (S_ISFIFO(st.st_mode)) {
        load_from_stdin(items, size);

        if (*size > 0)
            return;
    }

    load_from_path(items, size);
}
