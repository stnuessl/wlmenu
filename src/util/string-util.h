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

#ifndef STRING_UTIL_H_
#define STRING_UTIL_H_

#include <alloca.h>
#include <ctype.h>
#include <string.h>

#define strconcat2a(dst_, s1_, s2_)                                            \
    do {                                                                       \
        const char *s1 = (s1_);                                                \
        const char *s2 = (s2_);                                                \
        size_t len1 = strlen(s1);                                              \
        size_t len2 = strlen(s2) + 1;                                          \
        char *dst = alloca(len1 + len2);                                       \
                                                                               \
        memcpy(dst, s1, len1);                                                 \
        memcpy(dst + len1, s2, len2);                                          \
                                                                               \
        *(dst_) = dst;                                                         \
    } while (0)

static inline int streq(const char *s1, const char *s2)
{
    return strcmp(s1, s2) == 0;
}

static inline int strneq(const char *s1, const char *s2, size_t n)
{
    return strncmp(s1, s2, n) == 0;
}

static inline char *strlower(char *s)
{
    for (char *p = s; *p != '\0'; ++p)
        *p = tolower(*p);

    return s;
}

static inline char *strupper(char *s)
{
    for (char *p = s; *p != '\0'; ++p)
        *p = toupper(*p);

    return s;
}

char *strconcat(const char **array, int size);

char *strconcat2(const char *s1, const char *s2);

char *strnconcat(char *buf, size_t buf_size, const char **array, int size);

char *strnconcat2(char *buf, size_t buf_size, const char *s1, const char *s2);

#endif /* STRING_UTIL_H_ */
