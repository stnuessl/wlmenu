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

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "proc-util.h"
#include "config.h"
#include "xmalloc.h"

static void config_load_from_str(struct config *c, char *str, size_t size)
{
}

void config_init(struct config *c)
{
    c->color_fg = 0xffffffff;
    c->color_bg = 0x000000ff;
    c->color_border = 0x888888ff;
    c->color_highlight_fg = 0x000000ff;
    c->color_highlight_bg = 0xffffffff;

    c->rows = 16;
}

void config_destroy(struct config *c)
{
    (void) c;
}

void config_load(struct config *c, const char *file)
{
    struct stat st;
    char *data;
    int fd, err;
    size_t size = 0;
    
    fd = open(file, O_RDONLY);
    if (fd < 0)
        die_error(errno, "Failed to open configuration file \"%s\"", file);

    err = fstat(fd, &st);
    if (err < 0)
        die_error(errno, "stat() failed");


    data = xmalloc(st.st_size + 1);
    
    do {
        ssize_t n = read(fd, data + size, st.st_size - size);
        if (n < 0) {
            if (errno == EINTR)
                continue;

            die_error(errno, "Failed to load configuration file \"%s\"", file);
        }

        size += n;
    } while (size < (size_t) st.st_size);

    data[st.st_size] = '\0';

    config_load_from_str(c, data, size);

    free(data);
    close(fd);
}

const char *config_get(const struct config *config, const char *key)
{
}

double config_get_as_double(const struct config *config, const char *key)
{
}

int config_get_as_int(const struct config *config, const char *key)
{
}
