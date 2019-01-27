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

#ifndef WIDGET_H_
#define WIDGET_H_

#include <stdint.h>
#include <stdbool.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include <cairo.h>

struct color {
    double red;
    double green;
    double blue;
    double alpha;
};

struct rectangle {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};

struct widget {
    FT_Library freetype;
    FT_Face face;
    
    cairo_t *cr;

    char **rows;
    size_t n_rows;
    size_t max_rows;
    size_t highlight;

    char str[32];
    size_t len;

    cairo_glyph_t *glyphs;
    int n_glyphs;
    int max_glyphs_output;
    int max_glyphs_input;

    int32_t row_height;
    struct rectangle output;
    struct rectangle input;

    double font_size;
    int32_t glyph_offset_x;
    int32_t glyph_offset_y;

    int32_t max_glyph_width;
    
    struct color foreground;
    struct color background;
    struct color highlight_foreground;
    struct color highlight_background;
    struct color border;
};

void widget_init(struct widget *w);

void widget_destroy(struct widget *w);

void widget_set_font(struct widget *w, const char *file);

void widget_set_font_size(struct widget *w, double size);

void widget_set_max_rows(struct widget *w, size_t max_rows);

void widget_configure(struct widget *w,
                      void *mem,
                      int32_t width,
                      int32_t height,
                      int32_t stride);

void widget_draw(struct widget *w);

void widget_clear_input_str(struct widget *w);

const char *widget_input_str(const struct widget *w);

size_t widget_input_strlen(const struct widget *w);

void widget_insert_char(struct widget *w, int c);

void widget_remove_char(struct widget *w);

const char *widget_highlight(const struct widget *w);

void widget_highlight_up(struct widget *w);

void widget_highlight_down(struct widget *w);

void widget_insert_row(struct widget *w, char *str);

bool widget_has_empty_row(const struct widget *w);

void widget_clear_rows(struct widget *w);

size_t widget_rows(const struct widget *w);

void widget_set_foreground(struct widget *w, uint32_t rgba);

void widget_set_background(struct widget *w, uint32_t rgba);

void widget_set_highlight_foreground(struct widget *w, uint32_t rgba);

void widget_set_highlight_background(struct widget *w, uint32_t rgba);

void widget_set_border(struct widget *w, uint32_t rgba);

void widget_area(const struct widget *w, struct rectangle *rect);

#endif /* WIDGET_H_ */
