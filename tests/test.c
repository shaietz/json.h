#include "json.h"

global int failures;

internal void check_roundtrip(Arena *a, char *in, char *want) {
    Arena saved = *a;
    JsonNode root;
    JsonError err;
    if (!json_parse(a, str8_cstr(in), &root, &err)) {
        printf("FAIL parse %s -> %s at %td\n", in, err.msg, err.pos);
        failures++;
    } else {
        Str8 got = json_serialize(a, &root);
        if (!str8_eq(got, str8_cstr(want))) {
            printf("FAIL %s\n  want %s\n  got  " STR8_FMT "\n", in, want, str8_varg(got));
            failures++;
        }
    }
    *a = saved;
}

internal void check_error(Arena *a, char *in, char *msg, isize line, isize col) {
    u8 *before = a->beg;
    JsonNode root;
    JsonError err = {0};
    if (json_parse(a, str8_cstr(in), &root, &err)) {
        printf("FAIL should reject: %s\n", in);
        failures++;
    } else if (strcmp(err.msg, msg) || err.line != line || err.col != col) {
        printf("FAIL %s\n  want %s @%td:%td\n  got  %s @%td:%td\n", in, msg, line, col, err.msg, err.line, err.col);
        failures++;
    }
    if (a->beg != before) {
        printf("FAIL arena not rolled back: %s\n", in);
        failures++;
    }
}

int main(void) {
    Arena arena = arena_make(MB(256));

    // valid input
    check_roundtrip(&arena, "null", "null");
    check_roundtrip(&arena, " \n\t true ", "true");
    check_roundtrip(&arena, "[]", "[]");
    check_roundtrip(&arena, "{}", "{}");
    check_roundtrip(&arena, "[1, -0.5, 1e3, 2E-2, 0, -0, 123456789012]",
                            "[1,-0.5,1000,0.02,0,-0,123456789012]");
    check_roundtrip(&arena, "0.1", "0.1");
    check_roundtrip(&arena, "1e400", "null");
    check_roundtrip(&arena, "{\"a\": [1, {\"b\": null}], \"c\": \"d\"}",
                            "{\"a\":[1,{\"b\":null}],\"c\":\"d\"}");
    check_roundtrip(&arena, "\"a\\\"b\\\\c\\/d\\n\\t\\b\\f\\r\"",
                            "\"a\\\"b\\\\c/d\\n\\t\\b\\f\\r\"");
    check_roundtrip(&arena, "\"\\u0041\\u00e9\\u20ac\"", "\"A\xc3\xa9\xe2\x82\xac\"");
    check_roundtrip(&arena, "\"\\ud83d\\ude00\"", "\"\xf0\x9f\x98\x80\"");      // surrogate pair
    check_roundtrip(&arena, "\"\\ud83dx\"", "\"\xef\xbf\xbdx\"");              // lone high
    check_roundtrip(&arena, "\"\\ude00\"", "\"\xef\xbf\xbd\"");                // lone low
    check_roundtrip(&arena, "\"\\u0000\\u001f\"", "\"\\u0000\\u001f\"");
    check_roundtrip(&arena, "\"\"", "\"\"");
    check_roundtrip(&arena, "{\"\\u006b\": 1}", "{\"k\":1}");

    // big array: exercises array growth
    {
        Arena saved = arena;
        Str8 src = S8("[");
        for (int i = 0; i < 10000; i++) src = str8_concat(&arena, src, i ? S8(",[1,\"x\"]") : S8("[1,\"x\"]"));
        src = str8_concat(&arena, src, S8("]"));
        JsonNode root;
        if (!json_parse(&arena, src, &root, 0) || root.array.len != 10000 ||
            json_at(json_at(&root, 9999), 1)->string.len != 1) {
            printf("FAIL big array\n");
            failures++;
        }
        arena = saved;
    }

    // lookups chain through null
    {
        Arena saved = arena;
        JsonNode root;
        json_parse(&arena, S8("{\"user\": {\"name\": \"shai\", \"tags\": [\"a\", \"b\"]}}"), &root, 0);
        JsonNode *name = json_get(json_get(&root, S8("user")), S8("name"));
        JsonNode *tag  = json_at(json_get(json_get(&root, S8("user")), S8("tags")), 1);
        JsonNode *miss = json_get(json_get(json_get(&root, S8("nope")), S8("x")), S8("y"));
        if (!name || !str8_eq(name->string, S8("shai")) || !tag || !str8_eq(tag->string, S8("b")) || miss) {
            printf("FAIL lookups\n");
            failures++;
        }
        arena = saved;
    }

    // not null-terminated: only the first 4 bytes are the input
    {
        JsonNode root;
        Str8 s = {(u8 *)"true garbage", 4};
        if (!json_parse(&arena, s, &root, 0) || root.type != JSON_BOOL) {
            printf("FAIL length-bounded input\n");
            failures++;
        }
    }

    // invalid input
    check_error(&arena, "",              "expected a value", 1, 1);
    check_error(&arena, "[1,]",          "expected a value", 1, 4);
    check_error(&arena, "[1 2]",         "expected ',' or ']'", 1, 4);
    check_error(&arena, "{\"a\" 1}",     "expected ':' after key", 1, 6);
    check_error(&arena, "{1: 2}",        "expected a string key", 1, 2);
    check_error(&arena, "{\"a\":1,}",    "expected a string key", 1, 8);
    check_error(&arena, "01",            "leading zero in number", 1, 2);
    check_error(&arena, "1.",            "expected digit after '.'", 1, 3);
    check_error(&arena, "1e",            "expected digit in exponent", 1, 3);
    check_error(&arena, "-",             "expected digit", 1, 2);
    check_error(&arena, "+1",            "unexpected character", 1, 1);
    check_error(&arena, "tru",           "invalid literal", 1, 1);
    check_error(&arena, "\"abc",         "unterminated string", 1, 1);
    check_error(&arena, "\"ab\\",        "unterminated string", 1, 1);
    check_error(&arena, "\"\\x\"",       "invalid escape sequence", 1, 2);
    check_error(&arena, "\"\\u12g4\"",   "expected 4 hex digits after \\u", 1, 6);
    check_error(&arena, "\"a\tb\"",      "control character in string", 1, 3);
    check_error(&arena, "[1]\n  x",      "unexpected character", 2, 3);
    check_error(&arena, "{\"a\":\n [1,\n  2 3]}", "expected ',' or ']'", 3, 5);
    check_error(&arena, "1 2",           "unexpected data after value", 1, 3);
    {
        // deep nesting is rejected instead of overflowing the stack
        Arena saved = arena;
        Str8 deep = {0};
        for (int i = 0; i < 100000; i++) deep = str8_concat(&arena, deep, S8("["));
        JsonNode root;
        JsonError err;
        if (json_parse(&arena, deep, &root, &err) || strcmp(err.msg, "nesting too deep")) {
            printf("FAIL deep nesting\n");
            failures++;
        }
        arena = saved;
    }
    {
        JsonNode root;
        JsonError err;
        Str8 nul = {(u8 *)"[1,\0 2]", 7};
        if (json_parse(&arena, nul, &root, &err) || strcmp(err.msg, "unexpected character")) {
            printf("FAIL embedded NUL\n");
            failures++;
        }
    }

    printf(failures ? "%d FAILED\n" : "all tests passed\n", failures);
    return failures != 0;
}