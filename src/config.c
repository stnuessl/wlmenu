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
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util/die.h"
#include "util/io.h"
#include "util/string-util.h"
#include "util/xalloc.h"

#include "config.h"

struct parser {
    struct config *config;
    const char *path;
    int line;
    char *cur;
};

static int entry_cmp(const struct config_entry *e1,
                     const struct config_entry *e2)
{
    int cmp = strcmp(e1->section, e2->section);
    if (cmp)
        return cmp;

    return strcmp(e1->key, e2->key);
}

static void merge(struct config_entry *dst,
                  struct config_entry *s1,
                  struct config_entry *e1,
                  struct config_entry *s2,
                  struct config_entry *e2)
{
    while (s1 < e1 && s2 < e2) {
        if (entry_cmp(s1, s2) <= 0)
            memcpy(dst++, s1++, sizeof(*dst));
        else
            memcpy(dst++, s2++, sizeof(*dst));
    }

    while (s1 < e1)
        memcpy(dst++, s1++, sizeof(*dst));

    while (s2 < e2)
        memcpy(dst++, s2++, sizeof(*dst));
}

static void
sort(struct config_entry *entries, int size, struct config_entry *buf)
{
    struct config_entry item;
    int n = 16;

    for (int i = 0; i < size; i += n) {
        int end = i + n;

        if (end > size)
            end = size;

        for (int j = i + 1; j < end; ++j) {
            int k = j;

            memcpy(&item, entries + j, sizeof(item));

            while (k > i && entry_cmp(entries + k - 1, &item) > 0) {
                memcpy(entries + k, entries + k - 1, sizeof(item));

                --k;
            }

            if (j != k)
                memcpy(entries + k, &item, sizeof(item));
        }
    }

    while (n < size) {
        struct config_entry *src = entries;
        struct config_entry *dst = buf;

        for (int i = 0; i < size; i += (n + n)) {
            int j = i + n;
            int k = j + n;

            if (j > size)
                j = size;

            if (k > size)
                k = size;

            merge(dst, src + i, src + j, src + j, src + k);
            dst += k - i;
        }

        n *= 2;

        src = buf;
        dst = entries;

        for (int i = 0; i < size; i += (n + n)) {
            int j = i + n;
            int k = j + n;

            if (j > size)
                j = size;

            if (k > size)
                k = size;

            merge(dst, src + i, src + j, src + j, src + k);
            dst += k - i;
        }

        n *= 2;
    }
}

static void config_sort_entries(struct config *c)
{
    int n = 2 * c->n;

    if (n > c->size)
        c->entries = realloc(c->entries, n * sizeof(*c->entries));

    sort(c->entries, c->n, c->entries + c->n);
}

static void config_add_entry(struct config *c,
                             const char *section,
                             const char *key,
                             const char *value)
{
    if (c->n >= c->size) {
        c->size = c->size * 2 - c->size / 2;
        c->entries = xrealloc(c->entries, c->size * sizeof(*c->entries));
    }

    c->entries[c->n].section = section;
    c->entries[c->n].key = key;
    c->entries[c->n].value = value;

    ++c->n;
}

static void parser_skip_comment(struct parser *parser)
{
    while (1) {
        switch (*parser->cur) {
        case '\0':
            return;
        case '\n':
            ++parser->line;
            ++parser->cur;
            return;
        default:
            break;
        }

        ++parser->cur;
    }
}

static void parser_end_line(struct parser *parser)
{
    while (1) {
        switch (*parser->cur) {
        case ';':
        case '#':
            ++parser->cur;

            parser_skip_comment(parser);
            return;
        case '\n':
            ++parser->line;
            ++parser->cur;
            return;
        case '\0':
            return;
        default:
            if (!isblank(*parser->cur)) {
                die("config: %s:%d: invalid non-space character \"%c\" at "
                    "end of line.\n",
                    parser->path, parser->line, *parser->cur);
            }

            break;
        }

        ++parser->cur;
    }
}

static const char *parser_read_section_name(struct parser *parser)
{
    char *section = NULL;

    /* Find start of section name string. */
    while (!section) {
        switch (*parser->cur) {
        case ' ':
        case '\t':
            break;
        case ';':
        case '#':
            die("config: %s:%d: invalid comment in section definition\n",
                parser->path, parser->line);
        case '\n':
            die("config: %s:%d: invalid newline in section definition\n",
                parser->path, parser->line);
        case '\0':
            die("config: %s:%d: unexpected end of file in section definition\n",
                parser->path, parser->line);
        default:
            if (!isalpha(*parser->cur)) {
                die("config: %s:%d: section names must begin with an "
                    "alphabetic character ([a-zA-Z]).\n",
                    parser->path, parser->line);
            }

            section = parser->cur;
        }

        ++parser->cur;
    }

    while (isalpha(*parser->cur))
        ++parser->cur;

    /* Handle end of section name string. */
    while (1) {
        switch (*parser->cur) {
        case ';':
        case '#':
            die("config: %s:%d: invalid comment \"%c\" after section name\n",
                parser->path, parser->line, *parser->cur);
        case '\n':
            die("config: %s:%d: invalid newline before ending of section "
                "definition\n",
                parser->path, parser->line);
        case '\0':
            die("config: %s:%d: unexpected end of line before ending of "
                "section \"%s\"\n",
                parser->path, parser->line, section);
        case ']':
            *parser->cur++ = '\0';
            parser_end_line(parser);
            return section;
        default:
            if (!isblank(*parser->cur)) {
                die("config: %s:%d: invalid character \"%c\" at end of "
                    "section name\n",
                    parser->path, parser->line, *parser->cur);
            }

            *parser->cur++ = '\0';

            while (isblank(*parser->cur))
                ++parser->cur;

            if (*parser->cur++ != ']') {
                die("config: %s:%d: invalid character \"%c\" at end of "
                    "section \"%s\" definition - expected \"]\"\n",
                    parser->path, parser->line, *parser->cur, section);
            }

            parser_end_line(parser);
            return section;
        }

        ++parser->cur;
    }
}

static const char *parser_read_key(struct parser *parser)
{
    const char *key = parser->cur;

    while (1) {
        switch (*parser->cur) {
        case ';':
        case '#':
            die("config: %s:%d: invalid comment in key definition\n",
                parser->path, parser->line);
        case '\n':
            die("config: %s:%d: invalid newline character in key definition\n",
                parser->path, parser->line);
        case ' ':
        case '\t':
            *parser->cur++ = '\0';

            while (isblank(*parser->cur))
                ++parser->cur;

            if (*parser->cur == '=' || *parser->cur == ':')
                ++parser->cur;

            return key;
        case ':':
        case '=':
            *parser->cur++ = '\0';
            return key;
        case '\0':
            die("config: %s:%d: unexpected end of file while parsing key "
                "\"%s\"\n",
                parser->path, parser->line, key);
        default:
            if (!isalnum(*parser->cur)) {
                die("config: %s:%d: invalid non-alphanumeric ([a-zA-Z0-9]) "
                    "character \"%c\" in definition of key\n",
                    parser->path, parser->line, *parser->cur);
            }

            break;
        }

        ++parser->cur;
    }
}

static const char *parser_read_value(struct parser *parser)
{
    const char *value = NULL;

    while (!value) {
        switch (*parser->cur) {
        case ';':
        case '#':
            die("config: %s:%d: invalid comment in value definition\n",
                parser->path, parser->line);
        case '\n':
            die("config: %s:%d: invalid newline character in definition of "
                "value\n",
                parser->path, parser->line);
        case '\0':
            die("config: %s:%d: unexpected end of file while parsing value "
                "\"%s\"\n",
                parser->path, parser->line, value);
        default:
            if (isblank(*parser->cur))
                break;

            if (!isgraph(*parser->cur)) {
                die("config: %s:%d: invalid non-graphical character \"%c\" "
                    "at start of value definition\n",
                    parser->path, parser->line, *parser->cur);
            }

            value = parser->cur;
            break;
        }

        ++parser->cur;
    }

    while (1) {
        switch (*parser->cur) {
        case '\n':
            *parser->cur++ = '\0';
            ++parser->line;
            return value;
        case '\0':
            return value;
        case ';':
        case '#':
            *parser->cur++ = '\0';
            parser_skip_comment(parser);
            return value;
        default:
            if (isblank(*parser->cur)) {
                *parser->cur++ = '\0';

                parser_end_line(parser);
                return value;
            }

            if (!isgraph(*parser->cur)) {
                die("config: %s:%d: invalid non-graphical character \"%c\" "
                    "in definition of value\n",
                    parser->path, parser->line, *parser->cur);
            }

            break;
        }

        ++parser->cur;
    }
}

static void parser_read_section(struct parser *parser)
{
    const char *section = parser_read_section_name(parser);

    while (1) {
        const char *key = NULL;
        const char *value;

        while (!key) {
            switch (*parser->cur) {
            case '#':
            case ';':
                parser_skip_comment(parser);
                continue;
            case '\n':
                ++parser->line;
                break;
            case '[':
            case '\0':
                return;
            default:
                if (isblank(*parser->cur))
                    break;

                if (!isalpha(*parser->cur)) {
                    die("config: %s:%d: invalid non-alphabetic ([a-zA-Z]) "
                        "first character \"%c\" for key\n",
                        parser->path, parser->line, *parser->cur);
                }

                key = parser_read_key(parser);
                continue;
            }

            ++parser->cur;
        }

        value = parser_read_value(parser);

        config_add_entry(parser->config, section, key, value);
    }
}

static void config_do_load(struct config *c, const char *path)
{
    struct parser parser;

    parser.config = c;
    parser.cur = c->mem;
    parser.path = path;
    parser.line = 1;

    while (1) {
        switch (*parser.cur++) {
        case '#':
        case ';':
            parser_skip_comment(&parser);
            break;
        case '\n':
            ++parser.line;
            break;
        case '\t':
        case ' ':
            break;
        case '[':
            parser_read_section(&parser);
            break;
        case '\0':
            return;
        default:
            die("config: %s:%d: invalid character \"%c\" outside of any "
                "section\n",
                parser.path, parser.line, parser.cur[-1]);
            break;
        }
    }
}

void config_init(struct config *c)
{
    memset(c, 0, sizeof(*c));
}

void config_destroy(struct config *c)
{
    if (c->entries)
        free(c->entries);

    if (c->mem)
        free(c->mem);
}

void config_load(struct config *c, const char *path)
{
    struct stat st;
    int fd, err;

    if (!path)
        return;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT)
            return;

        die_error(errno, "Failed to open configuration file \"%s\"", path);
    }

    err = fstat(fd, &st);
    if (err < 0)
        die_error(errno, "Failed to retrieve status of configuration file");

    /* Read in configuration file as a string */
    c->mem = xmalloc(st.st_size + 1);

    err = io_read(fd, c->mem, st.st_size);
    if (err < 0)
        die_error(errno, "Failed to read configuration file \"%s\"", path);

    c->mem[st.st_size] = '\0';

    /* 80 characters per line seems to be a reasonable assumption. */
    c->size = st.st_size / 80;
    c->n = 0;
    c->entries = xmalloc(c->size * sizeof(*c->entries));

    config_do_load(c, path);
    config_sort_entries(c);

    close(fd);
}

static int config_get_index(const struct config *c, const char *str)
{
    int begin = 0, end = c->n;
    const char *section = str, *key = strchr(str, '.');
    size_t len;

    if (!key)
        return -1;

    /* Get the length of 'section' and move 'key' to its real beginning */
    len = key++ - section;

    /* Perform binary search */
    while (begin < end) {
        int i = begin + (end - begin) / 2;
        int cmp = strncasecmp(section, c->entries[i].section, len);
        if (!cmp)
            cmp = strcasecmp(key, c->entries[i].key);

        if (cmp < 0)
            end = i;
        else if (cmp > 0)
            begin = i + 1;
        else
            return i;
    }

    return -1;
}

const char *
config_get_str(const struct config *c, const char *str, const char *sub)
{
    int index;

    index = config_get_index(c, str);
    if (index < 0)
        return sub;

    return c->entries[index].value;
}

bool config_get_bool(const struct config *c, const char *str, bool sub)
{
    char *value;
    int index;

    index = config_get_index(c, str);
    if (index < 0)
        return sub;

    value = strdupa(c->entries[index].value);

    strlower(value);

    if (streq(value, "1") || streq(value, "y") || streq(value, "yes"))
        return true;

    if (streq(value, "0") || streq(value, "n") || streq(value, "no"))
        return false;

    return sub;
}

int config_get_int(const struct config *c, const char *str, int sub)
{
    char *p;
    long val;
    int index, err;

    index = config_get_index(c, str);
    if (index < 0)
        return sub;

    errno = 0;
    val = strtol(c->entries[index].value, &p, 0);
    if (errno)
        err = errno;
    else if (*p != '\0')
        err = EINVAL;
    else if (val < (long) INT_MIN || val > (long) INT_MAX)
        err = ERANGE;
    else
        err = 0;

    if (err) {
        die_error(err, "config: invalid integer value \"%s\" for key \"%s\"",
                  c->entries[index].value, str);
    }

    return (uint32_t) val;
}

uint32_t config_get_u32(const struct config *c, const char *str, uint32_t sub)
{
    char *p;
    long val;
    int index, err;

    index = config_get_index(c, str);
    if (index < 0)
        return sub;

    errno = 0;
    val = strtol(c->entries[index].value, &p, 0);
    if (errno)
        err = errno;
    else if (*p != '\0')
        err = EINVAL;
    else if (val < (long) 0 || val > (long) UINT32_MAX)
        err = ERANGE;
    else
        err = 0;

    if (err) {
        die_error(err, "config: invalid integer value \"%s\" for key \"%s\"",
                  c->entries[index].value, str);
    }

    return (uint32_t) val;
}

double config_get_double(const struct config *c, const char *str, double sub)
{
    char *p;
    double val;
    int index, err;

    index = config_get_index(c, str);
    if (index < 0)
        return sub;

    errno = 0;
    val = strtod(c->entries[index].value, &p);
    if (errno)
        err = errno;
    else if (*p != '\0')
        err = EINVAL;
    else
        err = 0;

    if (err) {
        die_error(err,
                  "config: invalid floating point value \"%s\" for key \"%s\"",
                  c->entries[index].value, str);
    }

    return val;
}
