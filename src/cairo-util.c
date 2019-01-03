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

#include "cairo-util.h"

void cairo_util_make_color_u32(uint32_t rgba, struct color *c)
{
    c->red = (double) ((rgba & 0xff000000) >> 24) / 255.0;
    c->green = (double) ((rgba & 0x00ff0000) >> 16) / 255.0;
    c->blue = (double) ((rgba & 0x0000ff00) >> 8) / 255.0;
    c->alpha = (double) (rgba & 0x000000ff) / 255.0;
}

void cairo_util_set_source(cairo_t *cairo, const struct color *c)
{
    cairo_set_source_rgba(cairo, c->red, c->green, c->blue, c->alpha);
}
