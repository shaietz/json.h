# json.h
![tests](https://github.com/shaietz/json.h/actions/workflows/test.yml/badge.svg)

A small JSON parser and serializer for C11, built on
[base.h](https://github.com/shaietz/base.h).

- Parses into an arena. One `arena_make` up front, nothing to free.
- Takes input as a `Str8`, so the text doesn't need to be null-terminated.
- Decodes strings fully: escapes, `\uXXXX` and surrogate pairs become UTF-8.
- Reports errors with a message, byte offset, line and column.
- Limits nesting depth, so hostile input can't overflow the stack.
- Passes all 318 cases of [JSONTestSuite](https://github.com/nst/JSONTestSuite),
  under AddressSanitizer and UndefinedBehaviorSanitizer.

## Example

```c
#include "json.h"

int main(void) {
    Arena arena = arena_make(MB(64));

    Str8 text = S8("{\"user\": {\"name\": \"shai\", \"tags\": [\"c\", \"json\"]}}");
    JsonNode root;
    JsonError err;
    if (!json_parse(&arena, text, &root, &err)) {
        printf("error at %td:%td: %s\n", err.line, err.col, err.msg);
        return 1;
    }

    JsonNode *name = json_get(json_get(&root, S8("user")), S8("name"));
    if (name && name->type == JSON_STRING) {
        printf("name: " STR8_FMT "\n", str8_varg(name->string));
    }

    JsonNode *tag = json_at(json_get(json_get(&root, S8("user")), S8("tags")), 1);
    if (tag) printf("second tag: " STR8_FMT "\n", str8_varg(tag->string));

    Str8 out = json_serialize(&arena, &root);
    printf(STR8_FMT "\n", str8_varg(out));
}
```

```
name: shai
second tag: json
{"user":{"name":"shai","tags":["c","json"]}}
```

Link with `-lm`.

## API

```c
b32       json_parse(Arena *a, Str8 src, JsonNode *out, JsonError *err);
JsonNode *json_get(JsonNode *obj, Str8 key);
JsonNode *json_at(JsonNode *arr, isize i);
Str8      json_serialize(Arena *a, JsonNode *node);
```

**`json_parse`** parses `src` into `out`, allocating from `a`. It returns 1 on
success. On failure it returns 0, fills `err` (if not null) and gives back
everything it allocated from the arena.

**`json_get`** returns an object's member by key, and **`json_at`** returns an
array's element by index. Both return null when the node is null, the wrong
type, or doesn't have that key or index, so lookups can be chained without
checks in between. If an object has duplicate keys, `json_get` returns the first.

**`json_serialize`** writes compact JSON into the arena as one `Str8`. Don't
allocate from the same arena while it runs. Infinity and NaN, which JSON can't
represent, are written as `null`.

### Data model

```c
typedef enum JsonType {
    JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING, JSON_ARRAY, JSON_OBJECT,
} JsonType;

struct JsonNode {
    JsonType type;
    union {
        b32        boolean;
        f64        number;
        Str8       string;
        JsonArray  array;   // Array(JsonNode):   .data, .len
        JsonObject object;  // Array(JsonMember): .data, .len, each {Str8 key; JsonNode value;}
    };
};

struct JsonError {
    char *msg;
    isize pos;   // byte offset into the source
    isize line;  // 1-based
    isize col;   // 1-based, in bytes
};
```

Arrays and objects can be iterated directly:

```c
for (isize i = 0; i < node->object.len; i++) {
    JsonMember *m = &node->object.data[i];
    printf(STR8_FMT "\n", str8_varg(m->key));
}
```

Strings and keys are copied into the arena, so the parsed tree doesn't depend
on the source text staying alive. Decoded strings can contain NUL bytes (from
`\u0000`), so use `.len` rather than treating them as C strings.

### Configuration

Define before including `json.h`:

| Macro            | Default | Meaning                                   |
|------------------|---------|-------------------------------------------|
| `JSON_MAX_DEPTH` | `512`   | Deepest nesting of arrays and objects allowed |

## Behavior on edge cases

The JSON spec leaves some cases up to the parser (the `i_` cases in
JSONTestSuite). This parser:

- **accepts** numbers too large or small for a double. They become infinity or 0.
- **accepts** lone surrogates such as `"\ud800"`. They decode to U+FFFD.
- **accepts** invalid UTF-8 inside strings, and passes the bytes through unchanged.
- **rejects** a leading byte-order mark, and UTF-16 input.

It also rejects a NUL byte anywhere in the input, rather than treating it as
the end of the text.

## Building and testing

```sh
make check            # everything below; run before committing
make test             # unit tests with ASan + UBSan
make test-release     # unit tests at -O2, no sanitizers
make suite            # JSONTestSuite with ASan + UBSan
make suite-verbose    # same, listing what happened to each i_ case
make CC=clang check   # use another compiler
```

JSONTestSuite is vendored in `tests/JSONTestSuite/`, pinned to the commit in
its `SOURCE` file. To update it, change `SUITE_COMMIT` in the `Makefile`, run
`make update-suite` and review the diff.

## License

MIT or public domain, at your choice, same as base.h. JSONTestSuite, in
`tests/JSONTestSuite/`, is MIT licensed by its authors.