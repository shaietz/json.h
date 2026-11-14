#ifndef JSON_H
#define JSON_H

#include "base.h"
#include <math.h>

#ifndef JSON_MAX_DEPTH
#define JSON_MAX_DEPTH 512
#endif

// ===========================================================================
// Types
// ===========================================================================

typedef enum JsonType {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} JsonType;

typedef struct JsonNode   JsonNode;
typedef struct JsonMember JsonMember;
typedef Array(JsonNode)   JsonArray;
typedef Array(JsonMember) JsonObject;

struct JsonNode {
    JsonType type;
    union {
        b32        boolean;
        f64        number;
        Str8       string;
        JsonArray  array;
        JsonObject object;
    };
};

struct JsonMember {
    Str8     key;
    JsonNode value;
};

typedef struct JsonError JsonError;
struct JsonError {
    char *msg;
    isize pos;
    isize line;
    isize col;
};

// ===========================================================================
// Lexer
// ===========================================================================

typedef enum JsonTokenType {
    JSON_TOK_EOF,
    JSON_TOK_ERROR,
    JSON_TOK_LBRACE,
    JSON_TOK_RBRACE,
    JSON_TOK_LBRACK,
    JSON_TOK_RBRACK,
    JSON_TOK_COLON,
    JSON_TOK_COMMA,
    JSON_TOK_STRING,
    JSON_TOK_NUMBER,
    JSON_TOK_TRUE,
    JSON_TOK_FALSE,
    JSON_TOK_NULL,
} JsonTokenType;

typedef struct JsonToken JsonToken;
struct JsonToken {
    JsonTokenType type;
    isize         pos;
    Str8          text;
    char          *err;
};

typedef struct JsonLexer JsonLexer;
struct JsonLexer {
    Str8  src;
    isize pos;
};

internal i32 json_lexer_peek(JsonLexer *l) {
    return l->pos < l->src.len ? l->src.buf[l->pos] : -1;
}

internal b32 json_is_digit(i32 c) {
    return c >= '0' && c <= '9';
}

internal i32 json_hex_value(i32 c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

internal JsonToken json_token(JsonTokenType type, isize pos, Str8 text) {
    JsonToken t = {type, pos, text, 0};
    return t;
}

internal JsonToken json_token_error(isize pos, char *msg) {
    JsonToken t = {JSON_TOK_ERROR, pos, {0}, msg};
    return t;
}

internal JsonToken json_lex_keyword(JsonLexer *l, isize start, Str8 word, JsonTokenType type) {
    Str8 rest = str8_slice(l->src, start, l->src.len);
    if (rest.len < word.len || !mem_eq(rest.buf, word.buf, word.len)) {
        return json_token_error(start, "invalid literal");
    }
    l->pos = start + word.len;
    return json_token(type, start, word);
}

internal JsonToken json_lex_string(JsonLexer *l, isize start) {
    for (;;) {
        i32 c = json_lexer_peek(l);
        if (c < 0) return json_token_error(start, "unterminated string");
        if (c == '"') break;
        if (c < 0x20) return json_token_error(l->pos, "control character in string");
        l->pos++;
        if (c != '\\') continue;

        c = json_lexer_peek(l);
        switch (c) {
            case '"': case '\\': case '/':
            case 'b': case 'f': case 'n': case 'r': case 't':
                l->pos++;
                break;
            case 'u':
                l->pos++;
                for (i32 i = 0; i < 4; i++, l->pos++) {
                    if (json_hex_value(json_lexer_peek(l)) < 0) {
                        return json_token_error(l->pos, "expected 4 hex digits after \\u");
                    }
                }
                break;
            case -1:
                return json_token_error(start, "unterminated string");
            default:
                return json_token_error(l->pos - 1, "invalid escape sequence");
        }
    }
    JsonToken t = json_token(JSON_TOK_STRING, start, str8_slice(l->src, start + 1, l->pos));
    l->pos++;
    return t;
}

internal JsonToken json_lex_number(JsonLexer *l, isize start) {
    l->pos = start;
    if (json_lexer_peek(l) == '-') l->pos++;

    if (json_lexer_peek(l) == '0') {
        l->pos++;
        if (json_is_digit(json_lexer_peek(l))) return json_token_error(l->pos, "leading zero in number");
    } else if (json_is_digit(json_lexer_peek(l))) {
        while (json_is_digit(json_lexer_peek(l))) l->pos++;
    } else {
        return json_token_error(l->pos, "expected digit");
    }

    if (json_lexer_peek(l) == '.') {
        l->pos++;
        if (!json_is_digit(json_lexer_peek(l))) return json_token_error(l->pos, "expected digit after '.'");
        while (json_is_digit(json_lexer_peek(l))) l->pos++;
    }

    i32 c = json_lexer_peek(l);
    if (c == 'e' || c == 'E') {
        l->pos++;
        c = json_lexer_peek(l);
        if (c == '+' || c == '-') l->pos++;
        if (!json_is_digit(json_lexer_peek(l))) return json_token_error(l->pos, "expected digit in exponent");
        while (json_is_digit(json_lexer_peek(l))) l->pos++;
    }

    return json_token(JSON_TOK_NUMBER, start, str8_slice(l->src, start, l->pos));
}

internal JsonToken json_lexer_next(JsonLexer *l) {
    for (;;) {
        i32 c = json_lexer_peek(l);
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
        l->pos++;
    }

    isize start = l->pos;
    i32   c     = json_lexer_peek(l);
    if (c < 0) return json_token(JSON_TOK_EOF, start, (Str8){0});
    l->pos++;

    Str8 val = str8_slice(l->src, start, l->pos);
    switch (c) {
        case '{': return json_token(JSON_TOK_LBRACE, start, val);
        case '}': return json_token(JSON_TOK_RBRACE, start, val);
        case '[': return json_token(JSON_TOK_LBRACK, start, val);
        case ']': return json_token(JSON_TOK_RBRACK, start, val);
        case ':': return json_token(JSON_TOK_COLON,  start, val);
        case ',': return json_token(JSON_TOK_COMMA,  start, val);
        case '"': return json_lex_string(l, start);
        case 't': return json_lex_keyword(l, start, S8("true"),  JSON_TOK_TRUE);
        case 'f': return json_lex_keyword(l, start, S8("false"), JSON_TOK_FALSE);
        case 'n': return json_lex_keyword(l, start, S8("null"),  JSON_TOK_NULL);
        default:
            if (c == '-' || json_is_digit(c)) return json_lex_number(l, start);
            return json_token_error(start, "unexpected character");
    }
}

// ===========================================================================
// Parser
// ===========================================================================
typedef struct JsonParser JsonParser;
struct JsonParser {
    JsonLexer lex;
    JsonToken cur;
    Arena    *a;
    isize     depth;
    char     *err;
    isize     errpos;
};

internal void json_parser_advance(JsonParser *p) {
    p->cur = json_lexer_next(&p->lex);
}

internal b32 json_parser_fail(JsonParser *p, char *msg) {
    if (!p->err) {
        if (p->cur.type == JSON_TOK_ERROR) msg = p->cur.err;
        p->err    = msg;
        p->errpos = p->cur.pos;
    }
    return 0;
}

internal b32 json_parser_accept(JsonParser *p, JsonTokenType type) {
    if (p->cur.type != type) return 0;
    json_parser_advance(p);
    return 1;
}

internal b32 json_parser_expect(JsonParser *p, JsonTokenType type, char *msg) {
    return json_parser_accept(p, type) || json_parser_fail(p, msg);
}

internal u32 json_read_hex4(u8 *s) {
    u32 r = 0;
    for (i32 i = 0; i < 4; i++) r = r << 4 | (u32)json_hex_value(s[i]);
    return r;
}

internal isize json_utf8_encode(u8 *dst, u32 cp) {
    if (cp < 0x80) {
        dst[0] = (u8)cp;
        return 1;
    } else if (cp < 0x800) {
        dst[0] = (u8)(0xc0 | cp >> 6);
        dst[1] = (u8)(0x80 | (cp & 0x3f));
        return 2;
    } else if (cp < 0x10000) {
        dst[0] = (u8)(0xe0 | cp >> 12);
        dst[1] = (u8)(0x80 | (cp >> 6 & 0x3f));
        dst[2] = (u8)(0x80 | (cp & 0x3f));
        return 3;
    } else {
        dst[0] = (u8)(0xf0 | cp >> 18);
        dst[1] = (u8)(0x80 | (cp >> 12 & 0x3f));
        dst[2] = (u8)(0x80 | (cp >> 6 & 0x3f));
        dst[3] = (u8)(0x80 | (cp & 0x3f));
        return 4;
    }
}

internal Str8 json_decode_string(Arena *a, Str8 raw) {
    Str8 out = {0};
    if (!raw.len) return out;

    out.buf = new(a, u8, raw.len, 0);
    for (isize i = 0; i < raw.len;) {
        u8 c = raw.buf[i++];
        if (c != '\\') {
            out.buf[out.len++] = c;
            continue;
        }
        c = raw.buf[i++];
        switch (c) {
            case 'b': out.buf[out.len++] = '\b'; break;
            case 'f': out.buf[out.len++] = '\f'; break;
            case 'n': out.buf[out.len++] = '\n'; break;
            case 'r': out.buf[out.len++] = '\r'; break;
            case 't': out.buf[out.len++] = '\t'; break;
            case 'u': {
                u32 cp = json_read_hex4(raw.buf + i);
                i += 4;
                if (cp >= 0xd800 && cp < 0xdc00 && i + 6 <= raw.len &&
                    raw.buf[i] == '\\' && raw.buf[i + 1] == 'u') {
                    u32 lo = json_read_hex4(raw.buf + i + 2);
                    if (lo >= 0xdc00 && lo < 0xe000) {
                        cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
                        i += 6;
                    }
                }
                if (cp >= 0xd800 && cp < 0xe000) cp = 0xfffd;
                out.len += json_utf8_encode(out.buf + out.len, cp);
            } break;
            default:
                out.buf[out.len++] = c;
        }
    }
    a->beg = out.buf + out.len;
    return out;
}

internal b32 json_parse_value(JsonParser *p, JsonNode *out);

internal b32 json_parse_array(JsonParser *p, JsonArray *out) {
    json_parser_advance(p);
    if (json_parser_accept(p, JSON_TOK_RBRACK)) return 1;
    do {
        JsonNode n;
        if (!json_parse_value(p, &n)) return 0;
        *array_push(p->a, out) = n;
    } while (json_parser_accept(p, JSON_TOK_COMMA));
    return json_parser_expect(p, JSON_TOK_RBRACK, "expected ',' or ']'");
}

internal b32 json_parse_object(JsonParser *p, JsonObject *out) {
    json_parser_advance(p);
    if (json_parser_accept(p, JSON_TOK_RBRACE)) return 1;
    do {
        if (p->cur.type != JSON_TOK_STRING) return json_parser_fail(p, "expected a string key");
        JsonMember m = {0};
        m.key = json_decode_string(p->a, p->cur.text);
        json_parser_advance(p);

        if (!json_parser_expect(p, JSON_TOK_COLON, "expected ':' after key")) return 0;
        if (!json_parse_value(p, &m.value)) return 0;
        *array_push(p->a, out) = m;
    } while (json_parser_accept(p, JSON_TOK_COMMA));
    return json_parser_expect(p, JSON_TOK_RBRACE, "expected ',' or '}'");
}

internal b32 json_parse_value(JsonParser *p, JsonNode *out) {
    *out = (JsonNode){0};
    JsonToken t = p->cur;

    if (t.type == JSON_TOK_LBRACE || t.type == JSON_TOK_LBRACK) {
        if (p->depth >= JSON_MAX_DEPTH) return json_parser_fail(p, "nesting too deep");
        p->depth++;
        b32 ok;
        if (t.type == JSON_TOK_LBRACE) {
            out->type = JSON_OBJECT;
            ok = json_parse_object(p, &out->object);
        } else {
            out->type = JSON_ARRAY;
            ok = json_parse_array(p, &out->array);
        }
        p->depth--;
        return ok;
    }

    switch (t.type) {
        case JSON_TOK_STRING:
            out->type   = JSON_STRING;
            out->string = json_decode_string(p->a, t.text);
            break;
        case JSON_TOK_NUMBER: {
            Arena scratch = *p->a;
            out->type   = JSON_NUMBER;
            out->number = strtod(str8_to_cstring(&scratch, t.text), 0);
        } break;
        case JSON_TOK_TRUE:  out->type = JSON_BOOL; out->boolean = 1; break;
        case JSON_TOK_FALSE: out->type = JSON_BOOL; out->boolean = 0; break;
        case JSON_TOK_NULL:  out->type = JSON_NULL;                   break;
        default:
            return json_parser_fail(p, "expected a value");
    }
    json_parser_advance(p);
    return 1;
}

internal b32 json_parse(Arena *a, Str8 src, JsonNode *out, JsonError *err) {
    Arena saved = *a;
    JsonParser p = {0};
    p.lex.src = src;
    p.a       = a;
    json_parser_advance(&p);

    if (json_parse_value(&p, out) &&
        json_parser_expect(&p, JSON_TOK_EOF, "unexpected data after value")) {
        return 1;
    }

    *a = saved;
    *out = (JsonNode){0};
    if (err) {
        JsonError e = {p.err, p.errpos, 1, 1};
        for (isize i = 0; i < p.errpos && i < src.len; i++) {
            if (src.buf[i] == '\n') {
                e.line++;
                e.col = 1;
            } else {
                e.col++;
            }
        }
        *err = e;
    }
    return 0;
}

internal JsonNode *json_get(JsonNode *obj, Str8 key) {
    if (!obj || obj->type != JSON_OBJECT) return 0;
    for (isize i = 0; i < obj->object.len; i++) {
        JsonMember *m = obj->object.data + i;
        if (str8_eq(m->key, key)) return &m->value;
    }
    return 0;
}

internal JsonNode *json_at(JsonNode *arr, isize i) {
    if (!arr || arr->type != JSON_ARRAY || i < 0 || i >= arr->array.len) return 0;
    return arr->array.data + i;
}

internal void json_emit(Arena *a, Str8 *out, Str8 s) {
    *out = str8_concat(a, *out, s);
}

internal void json_emit_number(Arena *a, Str8 *out, f64 d) {
    if (!isfinite(d)) {
        json_emit(a, out, S8("null"));
        return;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.15g", d);
    if (strtod(buf, 0) != d) snprintf(buf, sizeof(buf), "%.17g", d);
    json_emit(a, out, str8_cstr(buf));
}

internal void json_emit_string(Arena *a, Str8 *out, Str8 s) {
    json_emit(a, out, S8("\""));
    isize run = 0;
    for (isize i = 0; i < s.len; i++) {
        u8   c   = s.buf[i];
        Str8 esc = {0};
        char hex[8];
        switch (c) {
            case '"':  esc = S8("\\\""); break;
            case '\\': esc = S8("\\\\"); break;
            case '\n': esc = S8("\\n");  break;
            case '\r': esc = S8("\\r");  break;
            case '\t': esc = S8("\\t");  break;
            case '\b': esc = S8("\\b");  break;
            case '\f': esc = S8("\\f");  break;
            default:
                if (c >= 0x20) continue;
                snprintf(hex, sizeof(hex), "\\u%04x", c);
                esc = str8_cstr(hex);
        }
        json_emit(a, out, str8_slice(s, run, i));
        json_emit(a, out, esc);
        run = i + 1;
    }
    json_emit(a, out, str8_slice(s, run, s.len));
    json_emit(a, out, S8("\""));
}

internal void json_emit_node(Arena *a, Str8 *out, JsonNode *node) {
    switch (node->type) {
    case JSON_NULL:   json_emit(a, out, S8("null"));                              break;
    case JSON_BOOL:   json_emit(a, out, node->boolean ? S8("true") : S8("false")); break;
    case JSON_NUMBER: json_emit_number(a, out, node->number);                      break;
    case JSON_STRING: json_emit_string(a, out, node->string);                      break;
    case JSON_ARRAY:
        json_emit(a, out, S8("["));
        for (isize i = 0; i < node->array.len; i++) {
            if (i) json_emit(a, out, S8(","));
            json_emit_node(a, out, node->array.data + i);
        }
        json_emit(a, out, S8("]"));
        break;
    case JSON_OBJECT:
        json_emit(a, out, S8("{"));
        for (isize i = 0; i < node->object.len; i++) {
            if (i) json_emit(a, out, S8(","));
            json_emit_string(a, out, node->object.data[i].key);
            json_emit(a, out, S8(":"));
            json_emit_node(a, out, &node->object.data[i].value);
        }
        json_emit(a, out, S8("}"));
        break;
    }
}

internal Str8 json_serialize(Arena *a, JsonNode *node) {
    Str8 out = {0};
    json_emit_node(a, &out, node);
    return out;
}

#endif // JSON_H