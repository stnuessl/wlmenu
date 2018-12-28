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

#ifndef XKB_H_
#define XKB_H_

#include <stdbool.h>

#include <xkbcommon/xkbcommon.h>

struct xkb {
    struct xkb_keymap *keymap;
    struct xkb_state *state;
    struct xkb_context *context;
};

void xkb_init(struct xkb *xkb);

void xkb_destroy(struct xkb *xkb);

bool xkb_keymap_ok(const struct xkb *xkb);

bool xkb_set_keymap(struct xkb *xkb, const char *desc);

xkb_keysym_t xkb_get_sym(struct xkb *xkb, uint32_t key);

void xkb_state_update(struct xkb *xkb, 
                      uint32_t mods_depressed, 
                      uint32_t mods_latched,
                      uint32_t mods_locked,
                      uint32_t group);

#endif /* XKB_H_ */

