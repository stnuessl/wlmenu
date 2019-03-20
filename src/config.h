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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct config_entry {
    const char *section;
    const char *key;
    const char *value;
};

struct config {
    char *mem;

    struct config_entry *entries;
    int n;
    int size;
};

void config_init(struct config *c);

void config_destroy(struct config *c);

void config_load(struct config *c, const char *path);

const char *
config_get_str(const struct config *c, const char *key, const char *sub);

bool config_get_bool(const struct config *c, const char *key, bool sub);

int config_get_int(const struct config *c, const char *str, int sub);

uint32_t config_get_u32(const struct config *c, const char *key, uint32_t sub);

double config_get_double(const struct config *c, const char *str, double sub);

#endif /* CONFIG_H_ */
