/*
MIT License

Copyright (c) 2023 Ezequias Moises dos Santos Silva

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef SNOT_H
#define SNOT_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

typedef int SNOT_BOOL;

#define SNOT_TRUE 1
#define SNOT_FALSE 0
#define __SNORT_RETURN_ERROR(result)                                           \
    {                                                                          \
        SNOT_RESULT __result = (result);                                       \
        if (__result != SNOT_OK)                                               \
            return __result;                                                   \
    }

#define SNOT_IMPLEMENTATION

#if defined(SNOT_STATIC) && !defined(SNOT_IMPLEMENTATION)
#error "Only SNOT_IMPLEMENTATION builds supports SNOT_STATIC"
#endif

#ifdef SNOT_STATIC
#define SNOT_DEF static
#else
#define SNOT_DEF extern
#endif

typedef enum _SNOT_NUMBER_TYPE
{
    SNOT_UNKOWN_NUMBER = 0,
    SNOT_DEC_NUMBER    = 1,
    SNOT_HEX_NUMBER    = 2,
    SNOT_OCT_NUMBER    = 3,
    SNOT_REAL_NUMBER   = 4
} SNOT_NUMBER_TYPE;

typedef enum _SNOT_RESULT
{
    SNOT_OK = 0,
    SNOT_REPEAT,

    SNOT_ERROR_NO_MEMORY = 1 << (sizeof(SNOT_NUMBER_TYPE) * 8 - 1),
    SNOT_ERROR_INVALID_CHARACTER,
    SNOT_ERROR_PARTIAL,
    SNOT_ERROR_TOKEN_TYPE_UNDEFINED
} SNOT_RESULT;

typedef struct _SNOT_PARSER SNOT_PARSER;
typedef struct _SNOT_CALLBACKS
{
    void *(*alloc)(size_t size);
    void (*free)(void *ptr);
    void *(*grow)(void *memory, size_t *size, size_t grow_size);
    void (*start_section)(SNOT_PARSER *p, size_t id, void *userdata);
    void (*end_section)(SNOT_PARSER *p, size_t id, void *userdata);
    void (*string)(SNOT_PARSER *p, size_t id, void *userdata);
    void (*number)(SNOT_PARSER *p, size_t id, void *userdata);
} SNOT_CALLBACKS;

#ifndef SNOT_IMPLEMENTATION

SNOT_DEF SNOT_PARSER *snot_create(SNOT_CALLBACKS cbs, void *userdata);
SNOT_DEF void snot_free(SNOT_PARSER *p);
SNOT_DEF SNOT_RESULT snot_parse(SNOT_PARSER *p, uint32_t c);
SNOT_DEF SNOT_RESULT snot_end(SNOT_PARSER *p);
SNOT_DEF size_t snot_parent(SNOT_PARSER *p, size_t id);
SNOT_DEF SNOT_RESULT snot_value(SNOT_PARSER *p,
                                size_t id,
                                const char **value,
                                size_t *length);
SNOT_DEF SNOT_RESULT snot_number_type(SNOT_PARSER *p,
                                      size_t id,
                                      SNOT_NUMBER_TYPE *numberType);

#else
typedef enum _SNOT_TOKEN_TYPE
{
    SNOT_TOKEN_TYPE_UNDEFINED,
    SNOT_TOKEN_TYPE_NUMBER,
    SNOT_TOKEN_TYPE_IDENTIFIER,
    SNOT_TOKEN_TYPE_STRING,
    SNOT_TOKEN_TYPE_SECTION,
    SNOT_TOKEN_TYPE_GROUP,
    SNOT_TOKEN_TYPE_CONTINUE
} SNOT_TOKEN_TYPE;

typedef struct _SNOT_TOKEN
{
    size_t start;
    size_t length;
    size_t parent;
    SNOT_TOKEN_TYPE type;
    SNOT_NUMBER_TYPE numberType;
} SNOT_TOKEN;

struct _SNOT_PARSER
{
    SNOT_CALLBACKS callbacks;
    void *userdata;

    SNOT_TOKEN_TYPE type;
    SNOT_NUMBER_TYPE numberType;
    size_t parent;

    SNOT_TOKEN *tokens;
    size_t token_count;
    size_t next_token;

    char *pool;
    size_t pool_size;

    size_t start;
    size_t current;
};

static SNOT_RESULT __snot_grow(SNOT_PARSER *p, void **m, size_t *ps, size_t g)
{
    void *new_mem;
    assert(g);
    new_mem = p->callbacks.grow(*m, ps, g);
    if (new_mem == NULL)
        return SNOT_ERROR_NO_MEMORY;
    *m = new_mem;
    return SNOT_OK;
}

static SNOT_BOOL __snot_is_valid(uint32_t c)
{
    return c != 0xFFFE && c != 0xFFFF && (c < 0xFDD0 || c > 0xFDEF);
}

static SNOT_BOOL __snot_is_reserved(uint32_t c)
{
    return c == '(' || c == ')' || c == ';' || c == ',' || c == '.';
}

static SNOT_RESULT __snot_append_token(SNOT_PARSER *p, const SNOT_TOKEN *token)
{
    size_t size = p->token_count * sizeof(SNOT_TOKEN);

    if (p->next_token >= p->token_count)
    {
        __SNORT_RETURN_ERROR(
            __snot_grow(p, (void **)&p->tokens, &size, sizeof(SNOT_TOKEN)));
        p->token_count = size / sizeof(SNOT_TOKEN);
    }

    p->parent            = p->next_token++;
    p->tokens[p->parent] = *token;

    return SNOT_OK;
}

static SNOT_RESULT __snot_pop_token(SNOT_PARSER *p)
{
    const size_t size     = p->token_count * sizeof(SNOT_TOKEN);
    const size_t avaiable = size - p->current;
    const size_t index    = p->next_token - 1;

    if (avaiable < sizeof(SNOT_TOKEN))
        return SNOT_ERROR_PARTIAL;

    p->next_token--;

    if (index == p->parent)
        p->parent = p->tokens[index].parent;

    assert(p->start == p->current);
    p->current = p->start = p->tokens[index].start;

    return SNOT_OK;
}

static SNOT_RESULT __snot_peek_token(SNOT_PARSER *p, size_t i, SNOT_TOKEN **t)
{
    if (p->next_token < i + 1)
        return SNOT_ERROR_PARTIAL;

    *t = &p->tokens[p->next_token - i - 1];
    return SNOT_OK;
}

static SNOT_RESULT __snot_append_code_point(SNOT_PARSER *p, uint32_t c)
{
    const size_t avaiable = p->pool_size - p->current;
    char *current;

    if (avaiable < sizeof(uint32_t))
        __SNORT_RETURN_ERROR(__snot_grow(
            p, (void **)&p->pool, &p->pool_size, sizeof(uint32_t) - avaiable));

    current = p->pool + p->current;
    if (c <= 0x7F)
    {
        *current = c;
        p->current++;
    }
    else if (c <= 0x7FF)
    {
        current[0] = 0xC0 | (c >> 6);   /* 110x_xxxx */
        current[1] = 0x80 | (c & 0x3F); /* 10xx_xxxx */
        p->current += 2;
    }
    else if (c <= 0xFFFF)
    {
        current[0] = 0xE0 | (c >> 12);         /* 1110_xxxx */
        current[1] = 0x80 | ((c >> 6) & 0x3F); /* 10xx_xxxx */
        current[2] = 0x80 | (c & 0x3F);        /* 10xx_xxxx */
        p->current += 3;
    }
    else if (c <= 0x10FFFF)
    {
        current[0] = 0xF0 | (c >> 18);          /* 1111_0xxx */
        current[1] = 0x80 | ((c >> 12) & 0x3F); /* 10xx_xxxx */
        current[2] = 0x80 | ((c >> 6) & 0x3F);  /* 10xx_xxxx */
        current[3] = 0x80 | (c & 0x3F);         /* 10xx_xxxx */
        p->current += 4;
    }
    assert(p->current <= p->pool_size);

    return SNOT_OK;
}

static void
__snot_start_section(SNOT_PARSER *p, size_t id, const SNOT_TOKEN *token)
{
    assert(p);
    assert(token);
    assert(token->type == SNOT_TOKEN_TYPE_SECTION);

    p->callbacks.start_section(p, id, p->userdata);
}

static void
__snot_end_section(SNOT_PARSER *p, size_t id, const SNOT_TOKEN *token)
{
    assert(p);
    assert(token);
    assert(token->type == SNOT_TOKEN_TYPE_SECTION);

    p->callbacks.end_section(p, id, p->userdata);
}

static SNOT_BOOL __snot_is_whitespace(uint32_t c)
{
    return c == ' ' || c == 0x00A0 || c == 0x1680 || c == 0x2000 ||
           c == 0x2001 || c == 0x2002 || c == 0x2003 || c == 0x2004 ||
           c == 0x2005 || c == 0x2006 || c == 0x2007 || c == 0x2008 ||
           c == 0x2009 || c == 0x200A || c == 0x202F || c == 0x205F ||
           c == 0x3000 || c == '\n' || c == '\r' || c == '\t';
}

static SNOT_RESULT __snot_consume(SNOT_PARSER *p, size_t count)
{
    while (count--)
    {
        SNOT_TOKEN *token;
        const size_t id = p->next_token - 1;

        __SNORT_RETURN_ERROR(__snot_peek_token(p, 0, &token));

        switch (token->type)
        {
        case SNOT_TOKEN_TYPE_SECTION:
            __snot_end_section(p, id, token);
            break;
        case SNOT_TOKEN_TYPE_NUMBER:
            p->callbacks.number(p, id, p->userdata);
            break;
        case SNOT_TOKEN_TYPE_IDENTIFIER:
        case SNOT_TOKEN_TYPE_STRING:
            p->callbacks.string(p, id, p->userdata);
            break;
        default:
            return SNOT_ERROR_INVALID_CHARACTER;
        }
        __snot_pop_token(p);
    }

    return SNOT_OK;
}

static SNOT_RESULT __snot_value(SNOT_PARSER *p, uint32_t c)
{
    size_t count = 1;
    assert(p);

    switch (c)
    {
    case '.':
        count = 3;
    case ';':
        count = (count < 2) ? 2 : count;
    case ',':
        return __snot_consume(p, count);
    case '(':
    {
        SNOT_TOKEN token;
        token.type = SNOT_TOKEN_TYPE_GROUP;
        assert(p->start == p->current);
        token.start = token.length = p->start;
        token.parent               = p->parent;
        __snot_append_token(p, &token);
        break;
    }
    case ')':
        do
        {
            const SNOT_TOKEN *pToken;
            __SNORT_RETURN_ERROR(
                __snot_peek_token(p, 0, (SNOT_TOKEN **)&pToken));

            if (pToken->type != SNOT_TOKEN_TYPE_GROUP)
                __snot_consume(p, 1);
            else
            {
                __SNORT_RETURN_ERROR(__snot_pop_token(p));
                break;
            }
        } while (SNOT_TRUE);
        break;
    case '"':
        p->type = SNOT_TOKEN_TYPE_STRING;
        break;
    case '\\':
    {
        SNOT_TOKEN *last;
        __SNORT_RETURN_ERROR(__snot_peek_token(p, 0, &last));

        if (last->type != SNOT_TOKEN_TYPE_STRING)
            return SNOT_ERROR_INVALID_CHARACTER;

        p->type = SNOT_TOKEN_TYPE_CONTINUE;
        break;
    }
    default:
        if (!__snot_is_valid(c))
            return SNOT_ERROR_INVALID_CHARACTER;

        if (__snot_is_whitespace(c))
            break;

        if (c >= '0' && c <= '9')
        {
            p->type       = SNOT_TOKEN_TYPE_NUMBER;
            p->numberType = SNOT_UNKOWN_NUMBER;
        }
        else
            p->type = SNOT_TOKEN_TYPE_IDENTIFIER;

        return __snot_append_code_point(p, c);
    }

    return SNOT_OK;
}

static SNOT_RESULT __snot_section(SNOT_PARSER *p)
{
    SNOT_TOKEN *last = NULL;

    size_t i = 0;
    do
    {
        if (__snot_peek_token(p, i, &last) == SNOT_OK)
        {
            if (last->type == SNOT_TOKEN_TYPE_IDENTIFIER ||
                last->type == SNOT_TOKEN_TYPE_STRING)
            {
                assert(last);

                /* expect a identifier or string as section name */
                if (last->type != SNOT_TOKEN_TYPE_IDENTIFIER &&
                    last->type != SNOT_TOKEN_TYPE_STRING)
                    return SNOT_ERROR_PARTIAL; /* TODO: other error code */

                /* previous token is now a section */
                last->type = SNOT_TOKEN_TYPE_SECTION;
                __snot_start_section(p, p->next_token - i - 1, last);
            }
        }
        else
            break;
        i++;
    } while (last->type == SNOT_TOKEN_TYPE_GROUP);

    return SNOT_OK;
}

static SNOT_RESULT __snot_identifier(SNOT_PARSER *p, uint32_t c)
{
    assert(p);
    assert(c);

    /* end of identifier */
    if (__snot_is_whitespace(c) || __snot_is_reserved(c))
    {
        SNOT_TOKEN token;

        __SNORT_RETURN_ERROR(__snot_append_code_point(p, '\0'));

        token.start  = p->start;
        token.length = p->current - p->start - 1;
        token.parent = p->parent;
        token.type   = SNOT_TOKEN_TYPE_IDENTIFIER;

        p->start = p->current;

        __SNORT_RETURN_ERROR(__snot_section(p));
        __SNORT_RETURN_ERROR(__snot_append_token(p, &token));

        p->type = SNOT_TOKEN_TYPE_UNDEFINED;

        return __snot_is_whitespace(c) ? SNOT_OK : SNOT_REPEAT;
    }

    return __snot_append_code_point(p, c);
}

static SNOT_RESULT __snot_escape_character(uint32_t *c)
{
    const char cc = *c;
    if (cc == '\'' || cc == '"' || cc == '?' || cc == '\\')
        ;
    else if (cc == 'a')
        *c = '\a';
    else if (cc == 'b')
        *c = '\b';
    else if (cc == 'f')
        *c = '\f';
    else if (cc == 'n')
        *c = '\n';
    else if (cc == 'r')
        *c = '\r';
    else if (cc == 't')
        *c = '\t';
    else if (cc == 'v')
        *c = '\v';
    else if (cc == 'e')
        *c = '\x1B';
    else
        return SNOT_ERROR_INVALID_CHARACTER;
    return SNOT_OK;
}

static SNOT_RESULT __snot_string(SNOT_PARSER *p, uint32_t c)
{
    SNOT_BOOL escape;
    assert(p);
    assert(c);
    escape = p->current > p->start && p->pool[p->current - 1] == '\\';
    if (c == '"' && !escape)
    {
        SNOT_TOKEN token;

        __SNORT_RETURN_ERROR(__snot_append_code_point(p, '\0'));

        token.start  = p->start;
        token.length = p->current - p->start - 1;
        token.parent = p->parent;
        token.type   = SNOT_TOKEN_TYPE_STRING;

        p->start = p->current;

        __SNORT_RETURN_ERROR(__snot_section(p));
        __SNORT_RETURN_ERROR(__snot_append_token(p, &token));

        p->type = SNOT_TOKEN_TYPE_UNDEFINED;

        return SNOT_OK;
    }
    if (escape)
    {
        __SNORT_RETURN_ERROR(__snot_escape_character(&c));
        p->current--;
    }
    return __snot_append_code_point(p, c);
}

static SNOT_RESULT __snot_continue(SNOT_PARSER *p, uint32_t c)
{
    if (__snot_is_whitespace(c))
        return SNOT_OK;
    else if (c == '"')
    {
        SNOT_TOKEN *last;
        __SNORT_RETURN_ERROR(__snot_peek_token(p, 0, &last));
        __SNORT_RETURN_ERROR(__snot_pop_token(p));

        p->start   = last->start;
        p->current = last->length + last->start;
        p->type    = SNOT_TOKEN_TYPE_STRING;

        return SNOT_OK;
    }
    return SNOT_ERROR_INVALID_CHARACTER;
}

static SNOT_BOOL isDigit(uint32_t c) { return c >= '0' && c <= '9'; }

static SNOT_BOOL isXDigit(uint32_t c)
{
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static SNOT_BOOL isOctDigit(uint32_t c) { return c >= '0' && c <= '7'; }

static SNOT_RESULT __snot_number(SNOT_PARSER *p, uint32_t c)
{
    assert(p);
    assert(c);
    assert(p->type == SNOT_TOKEN_TYPE_NUMBER);

    if (p->numberType == SNOT_UNKOWN_NUMBER)
    {
        if (p->current <= p->start && p->pool[p->current - 1] == '0')
        {
            if (c == 'x' || c == 'X')
                p->numberType = SNOT_HEX_NUMBER;
            else
                p->numberType = SNOT_OCT_NUMBER;
        }
        else
            p->numberType = SNOT_DEC_NUMBER;
    }

    if (!(c == '.' && p->numberType == SNOT_DEC_NUMBER) &&
        (__snot_is_whitespace(c) || __snot_is_reserved(c)))
    {
        SNOT_TOKEN token;
        const SNOT_BOOL dot = __snot_is_whitespace(c) &&
                              p->current > p->start &&
                              p->pool[p->current - 1] == '.';

        if (dot)
            p->pool[p->current - 1] = '\0';
        else
            __SNORT_RETURN_ERROR(__snot_append_code_point(p, '\0'));

        token.start      = p->start;
        token.length     = p->current - p->start - 1;
        token.parent     = p->parent;
        token.type       = SNOT_TOKEN_TYPE_NUMBER;
        token.numberType = p->numberType;

        p->start = p->current;

        __SNORT_RETURN_ERROR(__snot_section(p));
        __SNORT_RETURN_ERROR(__snot_append_token(p, &token));

        p->type = SNOT_TOKEN_TYPE_UNDEFINED;

        if (dot)
            __snot_consume(p, 3);

        return __snot_is_whitespace(c) ? SNOT_OK : SNOT_REPEAT;
    }

    switch (p->numberType)
    {
    case SNOT_DEC_NUMBER:
        if (c == '.')
            p->numberType = SNOT_REAL_NUMBER;
        else if (!isDigit(c))
            return SNOT_ERROR_INVALID_CHARACTER;
        break;
    case SNOT_REAL_NUMBER:
        if (!isDigit(c))
            return SNOT_ERROR_INVALID_CHARACTER;
        break;
    case SNOT_HEX_NUMBER:
        if (!isXDigit(c))
            return SNOT_ERROR_INVALID_CHARACTER;
        break;
    case SNOT_OCT_NUMBER:
        if (!isOctDigit(c))
            return SNOT_ERROR_INVALID_CHARACTER;
        break;
    case SNOT_UNKOWN_NUMBER:
    default:
        return SNOT_ERROR_TOKEN_TYPE_UNDEFINED;
    }

    return __snot_append_code_point(p, c);
}

SNOT_DEF SNOT_RESULT snot_parse(SNOT_PARSER *p, uint32_t c)
{
    SNOT_RESULT result;
    assert(p);

    do
        switch (p->type)
        {
        case SNOT_TOKEN_TYPE_UNDEFINED:
            result = __snot_value(p, c);
            break;
        case SNOT_TOKEN_TYPE_IDENTIFIER:
            result = __snot_identifier(p, c);
            break;
        case SNOT_TOKEN_TYPE_STRING:
            result = __snot_string(p, c);
            break;
        case SNOT_TOKEN_TYPE_NUMBER:
            result = __snot_number(p, c);
            break;
        case SNOT_TOKEN_TYPE_CONTINUE:
            result = __snot_continue(p, c);
            break;
        default:
            return SNOT_ERROR_TOKEN_TYPE_UNDEFINED;
        }
    while (result == SNOT_REPEAT);

    return result;
}

SNOT_DEF SNOT_RESULT snot_end(SNOT_PARSER *p)
{
    if (p->start != p->current)
        __SNORT_RETURN_ERROR(snot_parse(p, ' '));

    while (p->next_token)
        __SNORT_RETURN_ERROR(__snot_consume(p, 1));

    return SNOT_OK;
}

SNOT_DEF size_t snot_parent(SNOT_PARSER *p, size_t id)
{
    if (p->token_count <= id)
        return -1;

    return p->tokens[id].parent;
}
SNOT_DEF SNOT_RESULT snot_number_type(SNOT_PARSER *p,
                                      size_t id,
                                      SNOT_NUMBER_TYPE *numberType)
{
    if (p->token_count <= id || p->tokens[id].type != SNOT_TOKEN_TYPE_NUMBER)
        return SNOT_ERROR_TOKEN_TYPE_UNDEFINED;

    if (numberType)
        *numberType = p->tokens[id].numberType;

    return SNOT_OK;
}

SNOT_DEF SNOT_RESULT snot_value(SNOT_PARSER *p,
                                size_t id,
                                const char **value,
                                size_t *length)
{
    if (p->token_count <= id)
        return SNOT_ERROR_TOKEN_TYPE_UNDEFINED;

    if (length)
        *length = p->tokens[id].length;

    if (value)
        *value = p->pool + p->tokens[id].start;

    return SNOT_OK;
}

SNOT_DEF SNOT_PARSER *snot_create(SNOT_CALLBACKS cbs, void *userdata)
{
    SNOT_PARSER *p = cbs.alloc(sizeof(SNOT_PARSER));

    p->callbacks   = cbs;
    p->userdata    = userdata;
    p->parent      = 0;
    p->type        = SNOT_TOKEN_TYPE_UNDEFINED;
    p->pool        = NULL;
    p->pool_size   = 0;
    p->start       = 0;
    p->current     = 0;
    p->tokens      = NULL;
    p->token_count = 0;
    p->next_token  = 0;
    p->parent      = -1;

    return p;
}

SNOT_DEF void snot_free(SNOT_PARSER *p)
{
    if (!p)
        return;

    p->callbacks.free(p->pool);
    p->callbacks.free(p->tokens);
    p->callbacks.free(p);
}

#endif
#endif
