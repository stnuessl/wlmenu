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

#include <sys/mman.h>

#include "xkb.h"

void xkb_init(struct xkb *xkb)
{
    xkb->context = NULL;
    xkb->keymap = NULL;
    xkb->state = NULL;
}

void xkb_destroy(struct xkb *xkb)
{
    if (xkb->context)
        xkb_context_unref(xkb->context);

    if (xkb->keymap)
        xkb_keymap_unref(xkb->keymap);

    if (xkb->state)
        xkb_state_unref(xkb->state);
}

bool xkb_keymap_ok(const struct xkb *xkb)
{
    return !!xkb->keymap;
}

bool xkb_set_keymap(struct xkb *xkb, const char *desc)
{
    struct xkb_context *context;
    struct xkb_keymap *keymap;
    struct xkb_state *state;

    context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!context)
        goto fail0;

    keymap = xkb_keymap_new_from_string(
        context, desc, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap)
        goto fail1;

    state = xkb_state_new(keymap);
    if (!state)
        goto fail2;

    if (xkb->context)
        xkb_context_unref(xkb->context);

    if (xkb->keymap)
        xkb_keymap_unref(xkb->keymap);

    if (xkb->state)
        xkb_state_unref(xkb->state);

    xkb->context = context;
    xkb->keymap = keymap;
    xkb->state = state;

    return true;

fail2:
    xkb_keymap_unref(keymap);
fail1:
    xkb_context_unref(context);
fail0:
    return false;
}

bool xkb_set_wl_keymap(struct xkb *xkb, int fd, size_t size)
{
    char *mem;
    bool ok;

    mem = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED)
        return false;

    ok = xkb_set_keymap(xkb, mem);

    munmap(mem, size);

    return ok;
}

xkb_keysym_t xkb_get_sym(struct xkb *xkb, uint32_t key)
{
    return xkb_state_key_get_one_sym(xkb->state, key + 8);
}

bool xkb_ctrl_active(const struct xkb *xkb)
{
    const char *name = XKB_MOD_NAME_CTRL;
    enum xkb_state_component type = XKB_STATE_MODS_EFFECTIVE;

    return xkb_state_mod_name_is_active(xkb->state, name, type) > 0;
}

void xkb_state_update(struct xkb *xkb,
                      uint32_t mods_depressed,
                      uint32_t mods_latched,
                      uint32_t mods_locked,
                      uint32_t group)
{
    (void) xkb_state_update_mask(xkb->state, mods_depressed, mods_latched,
                                 mods_locked, 0u, 0u, group);
}

bool xkb_keysym_is_modifier(xkb_keysym_t symbol)
{
   return symbol >= XKB_KEY_Shift_L && symbol <= XKB_KEY_Hyper_R;
}

