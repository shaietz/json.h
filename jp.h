#define ARENA_IMPLEMENTATION
#include "arena.h"
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* ========= LEXER ========= */

typedef struct Lexer {
    const char *src; size_t pos;
} Lexer;

typedef enum TokenType {
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACK,
    TOKEN_RBRACK,
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,
    TOKEN_EOF,
    TOKEN_ERROR,
} TokenType;

typedef struct Token {
    TokenType type;
    size_t pos;
    union {
        struct { const char *ptr; size_t len; } string;
        double number;
        const char *errmsg;
    };
} Token;

static Token lexer_make_token(TokenType type, size_t pos) {
    return (Token){.type = type, .pos = pos};
}

static Token lexer_make_error(size_t pos, const char *msg) {
    return (Token){.type = TOKEN_ERROR, .pos = pos, .errmsg = msg};
}

static bool lexer_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static Token lexer_lex_keyword(Lexer *l, size_t start, const char *word, TokenType type) {
    size_t n = strlen(word);
    if (strncmp(l->src + start, word, n) != 0) {
        return lexer_make_error(start, "unexpected keyword");
    }
    l->pos = start + n;
    return lexer_make_token(type, start);
}

static Token lexer_lex_string(Lexer *l, size_t start) {
    for (;;) {
        char c = l->src[l->pos];
        if (c == '\0') return lexer_make_error(start, "unterminated string");
        if (c == '"') break;
        if (c == '\\') {
            if (l->src[l->pos + 1] == '\0') return lexer_make_error(start, "unterminated string");
            l-> pos += 2;
            continue;
        }
        l->pos++;
    }

    Token t = lexer_make_token(TOKEN_STRING, start);
    t.string.ptr = l->src + start + 1;
    t.string.len = l->pos - (start + 1);
    l->pos++;
    return t;
}

static Token lexer_lex_number(Lexer *l, size_t start) {
    l->pos = start;
    
    if (l->src[l->pos] == '-') l->pos++;

    if (l->src[l->pos] == '0') {
        l->pos++;
        if (isdigit((unsigned char)l->src[l->pos])) return lexer_make_error(start, "leading zero");
    } else if (l->src[l->pos] >= '1' && l->src[l->pos] <= '9') {
        while (isdigit((unsigned char)l->src[l->pos])) l->pos++;
    } else {
        return lexer_make_error(start, "expected integer");
    }

    if (l->src[l->pos] == '.') {
        l->pos++;
        if (!isdigit((unsigned char)l->src[l->pos])) return lexer_make_error(start, "expected fraction");
        while (isdigit((unsigned char)l->src[l->pos])) l->pos++;
    }

    if (l->src[l->pos] == 'e' || l->src[l->pos] == 'E') {
        l->pos++;
        if (l->src[l->pos] == '-' || l->src[l->pos] == '+') l->pos++;
        if (!isdigit((unsigned char)l->src[l->pos])) return lexer_make_error(start, "expected exponent");
        while (isdigit((unsigned char)l->src[l->pos])) l->pos++;
    }

    double num = strtod(l->src + start, NULL);
    Token t = lexer_make_token(TOKEN_NUMBER, start);
    t.number = num;
    return t;
}


static Token lexer_next_token(Lexer *l) {
    while(lexer_is_space(l->src[l->pos])) l->pos++;

    const size_t start = l->pos;
    char c = l->src[l->pos];

    if (c == '\0') return lexer_make_token(TOKEN_EOF, start);
    l->pos++;

    switch (c) {
        case '{':  return lexer_make_token(TOKEN_LBRACE, start);
        case '}':  return lexer_make_token(TOKEN_RBRACE, start);
        case '[':  return lexer_make_token(TOKEN_LBRACK, start);
        case ']':  return lexer_make_token(TOKEN_RBRACK, start);
        case ':':  return lexer_make_token(TOKEN_COLON, start);
        case ',':  return lexer_make_token(TOKEN_COMMA, start);

        case '"': return lexer_lex_string(l, start);

        case 't':  return lexer_lex_keyword(l, start, "true", TOKEN_TRUE);
        case 'f':  return lexer_lex_keyword(l, start, "false", TOKEN_FALSE);
        case 'n':  return lexer_lex_keyword(l, start, "null", TOKEN_NULL);
        
        default:
            if (c == '-' || isdigit((unsigned char)c)) return lexer_lex_number(l, start);
            return lexer_make_error(start, "unexpected character");
    }
}

/* ========= JSON TYPE ========= */

typedef enum { JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING, JSON_ARRAY, JSON_OBJECT } JsonType;

typedef struct JsonNode JsonNode;
typedef struct JsonMember JsonMember;

typedef struct { char *ptr; size_t len; } JsonString;
typedef struct { JsonNode *items; size_t count; size_t capacity; } JsonArray;
typedef struct { JsonMember *items; size_t count; size_t capacity; } JsonObject;
typedef struct { const char *msg; size_t pos; } JsonError;

struct JsonNode {
    JsonType type;
    union {
        bool       boolean;
        double     number;
        JsonString string;
        JsonArray  array;
        JsonObject object;
    };
};

struct JsonMember { JsonString key; JsonNode value; };

/* ========= PARSER ========= */

typedef struct Parser { 
    Lexer lex;
    Token cur; 
    Arena *a;  
    const char *errmsg;
    size_t errpos;
} Parser;

static void parser_advance(Parser *p) {
    p->cur = lexer_next_token(&p->lex);
}

static bool parser_fail(Parser *p, const char *msg) {
    if (!p->errmsg) {
        if (p->cur.type == TOKEN_ERROR) msg = p->cur.errmsg;
        p->errmsg = msg;
        p->errpos = p->cur.pos;
    }
    return false;
}

static bool parser_accept(Parser *p, TokenType t) {
    if (p->cur.type != t) return false;
    parser_advance(p);
    return true;
}

static bool parser_expect(Parser *p, TokenType t, const char *msg) {
    return parser_accept(p, t) || parser_fail(p, msg);
}

static JsonString parser_copy_string(Parser *p, Token t) {
    char *buf = arena_alloc(p->a, t.string.len + 1);
    memcpy(buf, t.string.ptr, t.string.len);
    buf[t.string.len] = '\0';
    return (JsonString){.ptr = buf, .len = t.string.len};
}

static bool parser_parse_value(Parser *p, JsonNode *out);

static bool parser_parse_array(Parser *p, JsonArray *out) {
    *out = (JsonArray){0};
    parser_advance(p);

    if (parser_accept(p, TOKEN_RBRACK)) return true;

    do {
        JsonNode n;
        if (!parser_parse_value(p, &n)) return false;
        arena_da_append(p->a, out, n);
    } while(parser_accept(p, TOKEN_COMMA));

    if (!parser_expect(p, TOKEN_RBRACK, "expected ',' or ']'")) return false;
    return true;
}

static bool parser_parse_object(Parser *p, JsonObject *out) {
    *out = (JsonObject){0};
    parser_advance(p);

    if (parser_accept(p, TOKEN_RBRACE)) return true;

    do {
        if (p->cur.type != TOKEN_STRING) return parser_fail(p, "expected a string key");
        JsonMember m = {.key = parser_copy_string(p, p->cur)};
        parser_advance(p);

        if (!parser_expect(p, TOKEN_COLON, "expected ':' after key")) return false;
        if (!parser_parse_value(p, &m.value)) return false;
        arena_da_append(p->a, out, m);
    } while (parser_accept(p, TOKEN_COMMA));

    if (!parser_expect(p, TOKEN_RBRACE, "expected ',' or '}'")) return false;
    return true;
}

static bool parser_parse_value(Parser *p, JsonNode *out) {
    Token t = p->cur;
    switch (t.type) {
        case TOKEN_LBRACE: out->type = JSON_OBJECT; return parser_parse_object(p, &out->object);
        case TOKEN_LBRACK: out->type = JSON_ARRAY;  return parser_parse_array(p, &out->array);

        case TOKEN_STRING: out->type = JSON_STRING; out->string = parser_copy_string(p, t); break;
        case TOKEN_NUMBER: out->type = JSON_NUMBER; out->number = t.number;                 break;
        case TOKEN_TRUE:   out->type = JSON_BOOL;   out->boolean = true;                    break;
        case TOKEN_FALSE:  out->type = JSON_BOOL;   out->boolean = false;                   break;
        case TOKEN_NULL:   out->type = JSON_NULL;                                           break;

        default: return parser_fail(p, "expected a value");
    }
    parser_advance(p);
    return true;
}


/* ========= PUBLIC FUNCTIONS ========= */

bool jp_parse(Arena *a, const char *src, JsonNode *out, JsonError *err) {
    Parser p = {.lex = {.src = src}, .a = a};
    parser_advance(&p);

    if (parser_parse_value(&p, out) && parser_expect(&p, TOKEN_EOF, "unexpected data after value")) return true;
    if (err) *err = (JsonError){.msg = p.errmsg, .pos = p.errpos};
    return false;
}

const JsonNode *jp_get(const JsonNode *obj, const char *key) {
    if (obj->type != JSON_OBJECT) return NULL;
    size_t len = strlen(key);
    for (size_t i = 0; i < obj->object.count; i++) {
        JsonMember *m = &obj->object.items[i];
        if (m->key.len == len && memcmp(m->key.ptr, key, len) == 0) return &m->value;
    }
    return NULL;
}

static void jp_print_number(FILE *f, double d) {
    char buf[32];
    snprintf(buf, sizeof buf, "%.15g", d);
    if (strtod(buf, NULL) != d) snprintf(buf, sizeof buf, "%.17g", d);
    fputs(buf, f);
}

static void jp_print_string(FILE *f, JsonString s) {
    fputc('"', f);
    for (size_t i = 0; i < s.len; i++) {
        unsigned char c = (unsigned char)s.ptr[i];
        switch (c)
        {
        case '"':  fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n", f); break;
        case '\r': fputs("\\r", f); break;
        case '\t': fputs("\\t", f); break;
        case '\b': fputs("\\b", f); break;
        case '\f': fputs("\\f", f); break;
        default:
            if (c < 0x20) fprintf(f, "\\u%04x", c);
            else          fputc(c, f);
        }
    }
    fputc('"', f);
}

void jp_print(FILE *f, const JsonNode *node) {
    switch (node->type) {
        case JSON_NULL: fputs("null", f);                           break;
        case JSON_NUMBER: jp_print_number(f, node->number);         break;
        case JSON_BOOL: fputs(node->boolean ? "true" : "false", f); break;
        case JSON_STRING: jp_print_string(f, node->string);         break;

        case JSON_ARRAY:
            fputc('[', f);
            for (size_t i = 0; i < node->array.count; i++) {
                if (i > 0) fputc(',', f);
                jp_print(f, &node->array.items[i]);
            }
            fputc(']', f);
        break;

        case JSON_OBJECT:
            fputc('{', f);
            for (size_t i = 0; i < node->object.count; i++) {
                jp_print_string(f, node->object.items[i].key);
                fputc(':', f);
                jp_print(f, &node->object.items[i].value);
                if (i < node->object.count - 1) fputc(',', f);
            }
            fputc('}', f);
        break;
    }
}
