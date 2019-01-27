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

#ifndef CONFIG_H_
#define CONFIG_H_

#include <stddef.h>
#include <stdint.h>

struct config {
    char *mem;
    size_t n;

    char *font_file;
    double font_size;

    size_t rows;

    uint32_t color_fg;
    uint32_t color_bg;
    uint32_t color_border;
    uint32_t color_highlight_fg;
    uint32_t color_highlight_bg;
};

void config_init(struct config *c);

void config_destroy(struct config *c);

void config_load(struct config *c, const char *file);

const char *config_get(const struct config *config, const char *key);

double config_get_as_double(const struct config *config, const char *key);

int config_get_as_int(const struct config *config, const char *key);

#endif /* CONFIG_H_ */
