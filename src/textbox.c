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
#include <string.h>

#include "textbox.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

void textbox_init(struct textbox *tb)
{
    memset(tb->input, '\0', ARRAY_SIZE(tb->input));
    tb->size = 0;
}

void textbox_destroy(struct textbox *tb)
{
    (void) tb;
}

void textbox_clear(struct textbox *tb)
{
    while (tb->size--)
        tb->input[tb->size] = '\0';
}

void textbox_insert(struct textbox *tb, int c)
{
    if (tb->size < ARRAY_SIZE(tb->input) - 1 && isascii(c))
        tb->input[tb->size++] = c;
}

void textbox_remove(struct textbox *tb)
{
    if (tb->size)
        tb->input[--tb->size] = '\0';
}

const char *textbox_str(const struct textbox *tb)
{
    return tb->input;
}
