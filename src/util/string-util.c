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

#include "string-util.h"

char *strconcat(const char **array, int size)
{
    char *s;
    size_t n = 1;

    for (int i = 0; i < size; ++i)
        n += strlen(array[i]);

    s = malloc(n);
    if (!s)
        return NULL;

    n = 0;

    for (int i = 0; i < size; ++i) {
        size_t len = strlen(array[i]);

        memcpy(s + n, array[i], len);
        n += len;
    }

    s[n] = '\0';

    return s;
}

char *strconcat2(const char *s1, const char *s2)
{
    const char *args[] = {s1, s2};

    return strconcat(args, 2);
}

char *strnconcat(char *buf, size_t buf_size, const char **array, int size)
{
    size_t n = 0;

    for (int i = 0; i < size && n < buf_size; ++i) {
        size_t len = strlen(array[i]);

        if (n + len >= buf_size)
            len = buf_size - n;

        memcpy(buf + n, array[i], len);
        n += len;
    }

    if (n >= buf_size)
        n = buf_size - 1;

    buf[n] = '\0';

    return buf;
}

char *strnconcat2(char *buf, size_t buf_size, const char *s1, const char *s2)
{
    const char *args[] = {s1, s2};

    return strnconcat(buf, buf_size, args, 2);
}
