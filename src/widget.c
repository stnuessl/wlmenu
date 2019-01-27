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
#include <ctype.h>

#include <cairo-ft.h>

#include "widget.h"
#include "xmalloc.h"
#include "proc-util.h"

#define ARRAY_SIZE(x) (sizeof((x)) / sizeof(*(x)))

#define GLYPH_BUFFER_SIZE 64

static void cairo_util_set_source(cairo_t *cairo, const struct color *c)
{
    cairo_set_source_rgba(cairo, c->red, c->green, c->blue, c->alpha);
}

static void cairo_util_rectangle(cairo_t *cairo, const struct rectangle *rect)
{
    cairo_rectangle(cairo, rect->x, rect->y, rect->width, rect->height);
}

static void cairo_util_show_glyphs(cairo_t *cairo,
                                   cairo_glyph_t *glyphs,
                                   int n_glyphs,
                                   int max_glyphs)
{
    int n = n_glyphs - max_glyphs;
    if (n > 0) {
        int32_t offset = glyphs[n].x - glyphs[0].x;

        glyphs += n;
        n_glyphs -= n;

        for (int i = 0; i < n_glyphs; ++i)
            glyphs[i].x -= offset;
    }

    cairo_show_glyphs(cairo, glyphs, n_glyphs);
}

static void color_set(struct color *c, uint32_t rgba)
{
    c->red   = (double) ((rgba & 0xff000000) >> 24) / 255.0;
    c->green = (double) ((rgba & 0x00ff0000) >> 16) / 255.0;
    c->blue  = (double) ((rgba & 0x0000ff00) >>  8) / 255.0;
    c->alpha = (double)  (rgba & 0x000000ff)        / 255.0;
}

static void widget_configure_font(struct widget *w, cairo_font_extents_t *ex)
{
    cairo_font_face_t *face;
    cairo_scaled_font_t *scaled_font;

    if (!w->face)
        die("No FreeType font face specified\n");

    if (w->font_size <= 0.0)
        die("Font size must be bigger than 0 - got %lf\n", w->font_size);

    face = cairo_ft_font_face_create_for_ft_face(w->face, FT_LOAD_DEFAULT);
    if (cairo_font_face_status(face) != CAIRO_STATUS_SUCCESS)
        die("Failed to create cairo font face\n");

    cairo_set_font_face(w->cr, face);
    cairo_set_font_size(w->cr, w->font_size);

    scaled_font = cairo_get_scaled_font(w->cr);
    if (cairo_scaled_font_status(scaled_font) != CAIRO_STATUS_SUCCESS)
        die("Failed to create scaled font\n");

    cairo_scaled_font_extents(scaled_font, ex);

    cairo_font_face_destroy(face);
}

static void widget_show_text(struct widget *w, 
                             int32_t x,
                             int32_t y,
                             const char *str,
                             size_t len,
                             int max_glyphs)
{
    cairo_glyph_t *glyphs = w->glyphs;
    int n_glyphs = w->n_glyphs;
    cairo_status_t status;

    if (!len)
        return;

    x += w->glyph_offset_x;
    y += w->glyph_offset_y;

    /* clang-format off */
    status = cairo_scaled_font_text_to_glyphs(cairo_get_scaled_font(w->cr),
                                              x,
                                              y,
                                              str,
                                              (int) len,
                                              &glyphs,
                                              &n_glyphs,
                                              NULL,
                                              NULL,
                                              NULL);
    /* clang-format on */
    if (status != CAIRO_STATUS_SUCCESS)
        die("Failed to retrieve glyphs for text input - %d\n", status);

    cairo_util_show_glyphs(w->cr, glyphs, n_glyphs, max_glyphs);

    if (glyphs != w->glyphs) {
        if (n_glyphs > w->n_glyphs) {
            cairo_glyph_free(w->glyphs);

            w->glyphs = glyphs;
            w->n_glyphs = n_glyphs;

            return;
        } 

        cairo_glyph_free(glyphs);
    }
}

static void widget_draw_output(struct widget *w)
{
    int32_t x = w->output.x;
    int32_t y = w->output.y;
    int32_t width = w->output.width;
    int32_t height = w->row_height;

    if (w->highlight >= w->n_rows)
        w->highlight = w->n_rows - 1;

    for (size_t i = 0; i < w->n_rows; ++i) {
        int32_t yh = y + height / 2;
        size_t len = strlen(w->rows[i]);

        if (i == w->highlight) {
            cairo_util_set_source(w->cr, &w->highlight_background);
            cairo_rectangle(w->cr, x, y, width, height);
            cairo_fill(w->cr);
            
            cairo_util_set_source(w->cr, &w->highlight_foreground);
            widget_show_text(w, x, yh, w->rows[i], len, w->max_glyphs_output); 
        } else {
            cairo_util_set_source(w->cr, &w->background);
            cairo_rectangle(w->cr, x, y, width, height);
            cairo_fill(w->cr);

            cairo_util_set_source(w->cr, &w->foreground);
            widget_show_text(w, x, yh, w->rows[i], len, w->max_glyphs_output);
        }

        y += height;
    }

    cairo_util_set_source(w->cr, &w->background);

    for (size_t i = w->n_rows; i < w->max_rows; ++i) {
        cairo_rectangle(w->cr, x, y, width, height);

        y += height;
    }

    cairo_fill(w->cr);

    cairo_util_set_source(w->cr, &w->border);
    cairo_util_rectangle(w->cr, &w->output);
    cairo_set_line_width(w->cr, 2.0);
    cairo_stroke(w->cr);
}


static void widget_draw_input(struct widget *w)
{
    int32_t x = w->input.x;
    int32_t y = w->input.y + w->input.height / 2;

    cairo_util_set_source(w->cr, &w->background);
    cairo_util_rectangle(w->cr, &w->input);
    cairo_fill_preserve(w->cr);

    cairo_util_set_source(w->cr, &w->border);
    cairo_set_line_width(w->cr, 2.0);
    cairo_stroke(w->cr);

    w->str[w->len++] = '_';

    cairo_util_set_source(w->cr, &w->foreground);
    widget_show_text(w, x, y, w->str, w->len, w->max_glyphs_input);
    
    w->str[--w->len] = '\0';
}

void widget_init(struct widget *w)
{
    FT_Error err;

    memset(w, 0, sizeof(*w));

    err = FT_Init_FreeType(&w->freetype);
    if (err != 0)
        die("FT_Init_FreeType(): FreeType initialization failed - %d\n", err);

    w->rows = xmalloc(10 * sizeof(*w->rows));
    w->max_rows = 10;
        
    w->glyphs = xmalloc(GLYPH_BUFFER_SIZE * sizeof(*w->glyphs));
    w->n_glyphs = GLYPH_BUFFER_SIZE;
}

void widget_destroy(struct widget *w)
{
    if (w->cr)
        cairo_destroy(w->cr);

    if (w->face)
        FT_Done_Face(w->face);

    free(w->glyphs);
    free(w->rows);

    FT_Done_Library(w->freetype);
}

void widget_set_font(struct widget *w, const char *file)
{
    FT_Error err;

    if (w->face)
        FT_Done_Face(w->face);

    err = FT_New_Face(w->freetype, file, 0, &w->face);
    if (err != 0)
        die("FT_New_Face(): failed to load font \"%s\" - %d\n", file, err);
}

void widget_set_font_size(struct widget *w, double size)
{
    w->font_size = size;
}

void widget_set_max_rows(struct widget *w, size_t max_rows)
{
    w->rows = xrealloc(w->rows, max_rows * sizeof(*w->rows));
    w->max_rows = max_rows;
}

void widget_configure(struct widget *w,
                      void *mem,
                      int32_t width,
                      int32_t height,
                      int32_t stride)
{
    cairo_surface_t *surface;
    cairo_font_extents_t ex;
    cairo_status_t status;

    if (w->cr)
        cairo_destroy(w->cr);

    /* clang-format off */
    surface = cairo_image_surface_create_for_data(mem,
                                                  CAIRO_FORMAT_ARGB32,
                                                  width,
                                                  height,
                                                  stride);
    /* clang-format on */

    status = cairo_surface_status(surface);
    if (status != CAIRO_STATUS_SUCCESS)
        die("widget: Failed to create cairo image surface - %d\n", status);

    w->cr = cairo_create(surface);

    status = cairo_status(w->cr);
    if (status != CAIRO_STATUS_SUCCESS)
        die("widget: Failed to create cairo rendering object - %d\n", status);

    cairo_surface_destroy(surface);

    cairo_set_source_rgba(w->cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(w->cr);
    
    widget_configure_font(w, &ex);
    
    w->row_height = 11 * ex.height / 10;

    w->output.width = width / 3.0;
    w->output.height = w->max_rows * w->row_height;
    w->input.width = width / 3.0;
    w->input.height = 1.5 * ex.height;

    w->output.y = (height - w->output.height - w->input.height) / 2;
    w->input.y = (w->output.y + w->output.height);

    w->output.x = (width - w->output.width) / 2;
    w->input.x = (width - w->input.width) / 2;

    w->glyph_offset_x = ex.max_x_advance;
    w->glyph_offset_y = (ex.ascent - ex.descent) / 2;
    w->max_glyph_width = ex.max_x_advance;

    w->max_glyphs_output = w->output.width / w->max_glyph_width - 1;
    w->max_glyphs_input = w->input.width / w->max_glyph_width - 1;
}

void widget_draw(struct widget *w)
{
    widget_draw_output(w);
    widget_draw_input(w);
}

void widget_clear_input_str(struct widget *w)
{
    while (w->len)
        w->str[--w->len] = '\0';
}

const char *widget_input_str(const struct widget *w)
{
    return w->str;
}

size_t widget_input_strlen(const struct widget *w)
{
    return w->len;
}

void widget_insert_char(struct widget *w, int c)
{
    if (w->len < ARRAY_SIZE(w->str) - 1 && isascii(c))
        w->str[w->len++] = c;
}

void widget_remove_char(struct widget *w)
{
    if (w->len)
        w->str[--w->len] = '\0';
}

const char *widget_highlight(const struct widget *w)
{
    if (!w->n_rows)
        return NULL;

    return w->rows[w->highlight];
}

void widget_highlight_up(struct widget *w)
{
    if (w->highlight)
        --w->highlight;
}

void widget_highlight_down(struct widget *w)
{
    if (w->highlight < w->n_rows)
        ++w->highlight;
}

void widget_insert_row(struct widget *w, char *str)
{
    if (w->n_rows < w->max_rows)
        w->rows[w->n_rows++] = str;
}

bool widget_has_empty_row(const struct widget *w)
{
    return w->n_rows < w->max_rows;
}

size_t widget_rows(const struct widget *w)
{
    return w->n_rows;
}

void widget_clear_rows(struct widget *w)
{
    w->n_rows = 0;
}

void widget_set_foreground(struct widget *w, uint32_t rgba)
{
    color_set(&w->foreground, rgba);
}

void widget_set_background(struct widget *w, uint32_t rgba)
{
    color_set(&w->background, rgba);
}

void widget_set_highlight_foreground(struct widget *w, uint32_t rgba)
{
    color_set(&w->highlight_foreground, rgba);
}

void widget_set_highlight_background(struct widget *w, uint32_t rgba)
{
    color_set(&w->highlight_background, rgba);
}

void widget_set_border(struct widget *w, uint32_t rgba)
{
    color_set(&w->border, rgba);
}

void widget_area(const struct widget *w, struct rectangle *rect)
{
    int32_t x = (w->output.x < w->input.x) ? w->output.x : w->input.x;
    int32_t y = (w->output.y < w->input.y) ? w->output.y : w->input.y;
    int32_t xw1 = w->output.x + w->output.width;
    int32_t xw2 = w->input.x + w->input.width;
    int32_t yh1 = w->output.y + w->output.height;
    int32_t yh2 = w->input.y + w->input.height;

    rect->x = x;
    rect->y = y;
    rect->width = ((xw1 > xw2) ? xw1 : xw2) - x;
    rect->height = ((yh1 > yh2) ? yh1 : yh2) - y;
}

