// base.h - v0.1 - a minimal single-file base layer for C11
// https://github.com/shaietz/base.h
//
// Licensed under MIT or public domain, at your choice; see the end of this file.
//
// A small foundation for "handmade"-style C: arena allocation, length-based
// strings, growable arrays, and a pattern for hash maps. Heavily inspired by
// Chris Wellons (nullprogram.com) and Ryan Fleury's base layer.
//
// ============================================================================
// QUICK START
// ============================================================================
//
//    #include "base.h"
//
//    typedef Array(Str8) Str8Array;
//
//    internal Str8Array split(Arena *a, Str8 s, u8 sep) {
//        Str8Array parts = {0};
//        for (Str8Cut c = {.tail = s}; c.tail.len;) {
//            c = str8_cut(c.tail, sep);
//            *array_push(a, &parts) = str8_trim(c.head);
//        }
//        return parts;
//    }
//
//    int main(void) {
//        Arena arena = arena_make(MB(64));
//
//        Str8Array fruits = split(&arena, S8("apple, banana ,cherry"), ',');
//        for (isize i = 0; i < fruits.len; i++) {
//            printf("%.*s\n", str8_varg(fruits.data[i]));
//        }
//    }
//
// ============================================================================
// CONVENTIONS
// ============================================================================
//
//   Types              PascalCase              Arena, Str8
//   Functions          module_snake_case       arena_alloc, str8_cut
//   Function macros    lowercase               new, array_push
//   Sizes and indices  signed (isize). `sizeof` and `alignof` are redefined
//                      to return isize; use them as normal.
//
//   Zero is initialization: {0} is always a valid, empty value. An empty Str8,
//   an empty Array and an empty map are all just zero.
//
//   Arena parameters:
//     Arena *a       results the caller keeps are allocated here
//     Arena scratch  passed BY VALUE, anything allocated in it is discarded
//                    automatically when the function returns
//
//   Out of memory is not recoverable: every allocation either succeeds or
//   prints "out of memory" and aborts. No allocation returns null.
//
// ============================================================================
// DOCUMENTATION
// ============================================================================
//
// ARENAS
//   arena_make(cap)          new top-level arena of cap bytes from malloc.
//                            Meant to be made once and never freed.
//   arena_sub(&parent, cap)  carve a child arena out of a parent
//   new(&a, T)               allocate one zeroed T
//   new(&a, T, n)            allocate n zeroed Ts
//   new(&a, T, n, 0)         allocate n Ts without zeroing
//   arena_alloc(...)         the function behind new(); rarely called directly
//
//   Save and restore by copying the struct:
//       Arena saved = a;  ...temporary work...  a = saved;
//
// STRINGS (Str8)
//   A pointer and a length. Not null-terminated, and does not own its memory:
//   slicing never copies. Null-terminate only at libc/OS boundaries.
//
//   S8("literal")            from a string literal (literals only!)
//   str8_cstr(cstr)          from a null-terminated C string
//   str8_eq(a, b)            byte-wise equality
//   str8_slice(s, beg, end)  bytes [beg, end) as a view into s
//   str8_trim(s)             strip leading and trailing whitespace
//   str8_cut(s, c)           split at the first c: {head, tail, ok}
//   str8_hash(s)             64-bit hash, for hash maps
//   str8_copy(&a, s)         copy into the arena
//   str8_to_cstring(&a, s)   null-terminated copy, for libc and the OS
//   str8_concat(&a, x, y)    x followed by y. If x is the arena's most recent
//                            allocation, y is appended in place (cheap loops)
//   str8_fmt(&a, fmt, ...)   printf into the arena
//
//   Printing: printf(STR8_FMT "\n", str8_varg(s));
//
//   Splitting idiom:
//       for (Str8Cut line = {.tail = text}; line.tail.len;) {
//           line = str8_cut(line.tail, '\n');
//           ... use line.head ...
//       }
//
// DYNAMIC ARRAYS
//   typedef Array(i32) I32Array;     declare one type per element type
//   I32Array xs = {0};               empty array
//   *array_push(&a, &xs) = 42;       append; returns pointer to the new
//                                    (zeroed) element
//
//   Grows inside the arena: in place when the array is the most recent
//   allocation, otherwise copied to a block twice the size. A slice of an
//   array is an Array with cap == len; pushing to it copies instead of
//   overwriting the original.
//
// HASH MAPS
//   A pattern, not an API: write one 15-line map per key/value type. See the
//   template in the HASH MAPS section below.
//
// ============================================================================
// NOTES AND GOTCHAS
// ============================================================================
//
//   - S8() only works on literals and arrays. On a pointer, lengthof()
//     gives the pointer's size, not the string's. Use str8_cstr() instead.
//   - array_push() evaluates its `arr` argument more than once.
//   - Maps and Str8 views don't copy their data: whatever they point at
//     must outlive them.
//   - Hash maps have no delete; they live and die with their arena.
//   - str8_hash() is not seeded. Don't use it on keys an attacker controls.
//
// ============================================================================
// VERSION HISTORY
// ============================================================================
//
//   0.1  (2026-11-14)  first public release
//

#ifndef BASE_H
#define BASE_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define internal      static
#define global        static
#define local_persist static

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;
typedef int32_t   b32;
typedef float     f32;
typedef double    f64;
typedef ptrdiff_t isize;
typedef size_t    usize;
typedef uintptr_t uptr;

#define sizeof(x)    (isize)sizeof(x)
#define alignof(x)   (isize)_Alignof(x)
#define countof(a)   (sizeof(a) / sizeof(*(a)))
#define lengthof(s)  (countof(s) - 1)

#define KB(n) ((isize)(n) << 10)
#define MB(n) ((isize)(n) << 20)

#if defined(_MSC_VER)
#  define ASSERT(c) do { if (!(c)) __debugbreak(); } while (0)
#else
#  define ASSERT(c) do { if (!(c)) __builtin_trap(); } while (0)
#endif


internal void *mem_copy(void *dst, void *src, isize n) {
    if (n) memcpy(dst, src, (usize)n);
    return dst;
}

internal void *mem_set(void *dst, u8 byte, isize n) {
    if (n) memset(dst, byte, (usize)n);
    return dst;
}

internal b32 mem_eq(void *a, void *b, isize n) {
    return !n || !memcmp(a, b, (usize)n);
}

// ===========================================================================
// Arenas
// ===========================================================================
typedef struct Arena Arena;
struct Arena {
    u8 *beg;
    u8 *end;
};

internal void arena_oom(void) {
    fputs("out of memory\n", stderr);
    abort();
}

internal void *arena_alloc(Arena *arena, isize size, isize align, isize count, b32 zero) {
    ASSERT(size > 0 && count >= 0);
    isize pad = (isize)(-(uptr)arena->beg & (uptr)(align - 1));
    isize available = arena->end - arena->beg - pad;
    if (available < 0 || count > available / size) {
        arena_oom();
    }
    u8 *result = arena->beg + pad;
    arena->beg += pad + count * size;
    if (zero) {
        mem_set(result, 0, count * size);
    }
    return result;
}

#define new(...)            newx(__VA_ARGS__,new4,new3,new2)(__VA_ARGS__)
#define newx(a,b,c,d,e,...) e
#define new2(a, t)          (t *)arena_alloc(a, sizeof(t), alignof(t), 1, 1)
#define new3(a, t, n)       (t *)arena_alloc(a, sizeof(t), alignof(t), n, 1)
#define new4(a, t, n, f)    (t *)arena_alloc(a, sizeof(t), alignof(t), n, f)

internal Arena arena_make(isize cap) {
    Arena arena = {0};
    arena.beg = (u8 *)malloc((usize)cap);
    if (!arena.beg) {
        arena_oom();
    }
    arena.end = arena.beg + cap;
    return arena;
}

internal Arena arena_sub(Arena *parent, isize cap) {
    Arena arena = {0};
    arena.beg = new(parent, u8, cap, 0);
    arena.end = arena.beg + cap;
    return arena;
}


// ===========================================================================
// Strings
// ===========================================================================
typedef struct Str8 Str8;
struct Str8 {
    u8   *buf;
    isize len;
};

#define S8(s)        ((Str8){(u8 *)(s), lengthof(s)})
#define STR8_FMT     "%.*s"
#define str8_varg(s) (int)(s).len, (char *)(s).buf

internal Str8 str8_cstr(char *cstr) {
    Str8 result = {(u8 *)cstr, cstr ? strlen(cstr) : 0};
    return result;
}

internal b32 str8_eq(Str8 a, Str8 b) {
    return a.len == b.len && mem_eq(a.buf, b.buf, a.len);
}

internal Str8 str8_slice(Str8 s, isize beg, isize end) {
    ASSERT(0 <= beg && beg <= end && end <= s.len);
    s.buf += beg;
    s.len   = end - beg;
    return s;
}

internal Str8 str8_trim(Str8 s) {
    while (s.len && (s.buf[0] == ' ' || (s.buf[0] >= '\t' && s.buf[0] <= '\r'))) {
        s.buf++;
        s.len--;
    }
    while (s.len && (s.buf[s.len - 1] == ' ' || (s.buf[s.len - 1] >= '\t' && s.buf[s.len - 1] <= '\r'))) {
        s.len--;
    }
    return s;
}

typedef struct Str8Cut Str8Cut;
struct Str8Cut {
    Str8 head;
    Str8 tail;
    b32  ok;
};

internal Str8Cut str8_cut(Str8 s, u8 c) {
    Str8Cut result = {0};
    for (isize i = 0; i < s.len; i++) {
        if (s.buf[i] == c) {
            result.head = str8_slice(s, 0, i);
            result.tail = str8_slice(s, i + 1, s.len);
            result.ok   = 1;
            return result;
        }
    }
    result.head = s;
    return result;
}

internal Str8 str8_copy(Arena *a, Str8 s) {
    Str8 result = {0};
    if (s.len) {
        result.buf = new(a, u8, s.len);
        result.len = s.len;
        mem_copy(result.buf, s.buf, s.len);
    }
    return result;
}

internal char *str8_to_cstring(Arena *a, Str8 s) {
    char *result = new(a, char, s.len + 1);
    mem_copy(result, s.buf, s.len);
    result[s.len] = '\0';
    return result;
}

internal Str8 str8_concat(Arena *a, Str8 head, Str8 tail) {
    if (!head.len) {
        return str8_copy(a, tail);
    }
    if (head.buf + head.len != a->beg) {
        head = str8_copy(a, head);
    }
    head.len += str8_copy(a, tail).len;
    return head;
}

internal Str8 str8_fmt(Arena *a, char *fmt, ...) {
    isize available = a->end - a->beg;
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf((char *)a->beg, (usize)available, fmt, args);
    va_end(args);
    if (len < 0 || len >= available) {
        arena_oom();
    }
    Str8 result = {a->beg, len};
    a->beg += len;
    return result;
}

internal u64 str8_hash(Str8 s) {
    u64 h = 0x100;
    for (isize i = 0; i < s.len; i++) {
        h ^= s.buf[i];
        h *= 1111111111111111111u;
    }
    return h;
}

// ===========================================================================
// Dynamic arrays: https://nullprogram.com/blog/2023/10/05/
// ===========================================================================
#define Array(T) struct { T *data; isize len; isize cap; }

internal void array_grow_(void *arr, isize item_size, Arena *arena) {
    struct { u8 *data; isize len; isize cap; } a;
    mem_copy(&a, arr, sizeof(a));

    isize align = alignof(max_align_t);
    if (!a.cap) {
        a.cap = 8;
        a.data = (u8 *)arena_alloc(arena, item_size, align, a.cap, 1);
    } else if (arena->beg == a.data + item_size * a.cap) {
        arena_alloc(arena, item_size, 1, a.cap, 1);
        a.cap *= 2;
    } else {
        u8 *data = (u8 *)arena_alloc(arena, 2 * item_size, align, a.cap, 1);
        mem_copy(data, a.data, item_size * a.len);
        a.data = data;
        a.cap *= 2;
    }
    mem_copy(arr, &a, sizeof(a));
}

#define array_push(arena, arr)                               \
    ((arr)->len >= (arr)->cap                                \
        ? array_grow_((arr), sizeof(*(arr)->data), (arena)), \
            (arr)->data + (arr)->len++                       \
        : (arr)->data + (arr)->len++)



// ===========================================================================
// Hash maps: https://nullprogram.com/blog/2023/09/30/
// write one per key/value type using this pattern (a 4-ary hash trie)
// ===========================================================================
//
//     typedef struct NameMap NameMap;
//     struct NameMap {
//         NameMap *child[4];
//         Str8     key;
//         i32      value;
//     };

//     internal i32 *namemap_upsert(NameMap **m, Str8 key, Arena *a) {
//         for (u64 h = str8_hash(key); *m; h <<= 2) {
//             if (str8_eq(key, (*m)->key)) return &(*m)->value;
//             m = &(*m)->child[h >> 62];
//         }
//         if (!a) return 0;
//         *m = new(a, NameMap);
//         (*m)->key = key;
//         return &(*m)->value;
//     }
//
//     NameMap *map = 0;                                  // empty map
//     *namemap_upsert(&map, key, &arena) += 1;           // insert or update
//     i32 *v = namemap_upsert(&map, key, 0);             // lookup only
//

#endif // BASE_H

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2026 Shai Etzion
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/