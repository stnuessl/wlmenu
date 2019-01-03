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

#include <errno.h>
#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>

#include "framebuffer.h"

void framebuffer_init(struct framebuffer *fb)
{
    fb->cairo = NULL;
    fb->mem = NULL;

    fb->size = 0;
    fb->width = 0;
    fb->height = 0;
    fb->stride = 0;
}

void framebuffer_destroy(struct framebuffer *fb)
{
    if (fb->cairo)
        cairo_destroy(fb->cairo);

    if (fb->mem)
        munmap(fb->mem, fb->size);
}

int framebuffer_configure(struct framebuffer *fb,
                          int fd,
                          int32_t width,
                          int32_t height,
                          cairo_format_t format)
{
    cairo_surface_t *surface;
    cairo_t *cairo;
    void *mem;
    size_t size;
    int32_t stride;
    int err;

    stride = cairo_format_stride_for_width(format, width);
    if (stride < 0)
        return -EINVAL;

    size = stride * height;

    err = ftruncate(fd, size);
    if (err < 0)
        return -errno;

    mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED)
        return -errno;

    /* clang-format off */
    surface = cairo_image_surface_create_for_data(mem,
                                                  format,
                                                  width,
                                                  height,
                                                  stride);
    /* clang-format on */

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        err = -ENOMEM;
        goto fail1;
    }

    cairo = cairo_create(surface);
    if (cairo_status(cairo) != CAIRO_STATUS_SUCCESS) {
        err = -ENOMEM;
        goto fail2;
    }

    cairo_surface_destroy(surface);

    if (fb->cairo)
        cairo_destroy(fb->cairo);

    if (fb->mem)
        munmap(fb->mem, fb->size);

    fb->cairo = cairo;

    fb->mem = mem;
    fb->size = size;

    fb->width = width;
    fb->height = height;
    fb->stride = stride;

    return 0;

fail2:
    cairo_surface_destroy(surface);
fail1:
    munmap(mem, size);

    return err;
}

cairo_t *framebuffer_cairo(struct framebuffer *fb)
{
    return fb->cairo;
}

size_t framebuffer_size(const struct framebuffer *fb)
{
    return fb->size;
}

int32_t framebuffer_width(const struct framebuffer *fb)
{
    return fb->width;
}

int32_t framebuffer_height(const struct framebuffer *fb)
{
    return fb->height;
}

int32_t framebuffer_stride(const struct framebuffer *fb)
{
    return fb->stride;
}
