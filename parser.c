#include "tokenizer.c"
#include <stdio.h>
#include "json_node.c"

void error(const char msg[]) {
    printf(msg);
    printf("\n");
}

typedef struct Parser Parser;
struct Parser {
    Arena a;
    TokenArray tokens;
    size_t pos;
};

void consumeToken(Parser *p) {
    p->pos++;
}

extern Token peekToken(Parser *p) {
    return (p->tokens.items[p->pos]);
}

bool accept(Parser *p, TokenType t) {
    if (peekToken(p).type == t) {
        consumeToken(p);
        return true;
    }
    return false;
}

bool expect(Parser *p, TokenType t) {
    if (accept(p, t)) {
        return true;
    }
    error("expect: unexpected symbol");
    return false;
}

bool parse_value(Parser *p, JsonNode *out); 

bool parse_string(Parser *p, JsonString *out) {
    char* string = peekToken(p).string_val;

    if (!expect(p, T_STRING)) return false;

    size_t length = strlen(string);
    out->ptr = arena_alloc(&p->a, length);
    memcpy(out->ptr, string, length);
    out->len = length;

    return true;
}

bool parse_number(Parser *p, double *out) {
    double val = peekToken(p).number_val;

    if (!expect(p, T_NUMBER)) return false;
    *out = val;
    return true;
}

bool parse_bool(Parser *p, bool *out) {
    TokenType t = peekToken(p).type;

    if (!(accept(p, T_TRUE) || accept(p, T_FALSE))) return false;

    *out = t == T_TRUE ? true : false;
    return true;
}

bool parse_array(Parser *p, JsonArray *out) {
    out->items = NULL;
    out->count = out->capacity = 0;

    if (!expect(p, T_LEFT_BRACKET)) return false;
    if (accept(p, T_RIGHT_BRACKET)) return true;

    do {
        JsonNode n;
        if (!parse_value(p, &n)) return false;
        arena_da_append(&p->a, out, n);
    } while (accept(p, T_COMMA));
    
    if (!expect(p, T_RIGHT_BRACKET)) return false;
    return true;
}

bool parse_object(Parser *p, JsonNode *out) {
    out->type = JSON_OBJECT;
    out->object.items = NULL;
    out->object.count = out->object.capacity = 0;

    expect(p, T_LEFT_BRACE);
    if (accept(p, T_RIGHT_BRACE)) return true;

    do {
        JsonMember m;
        if (!parse_string(p, &m.key)) return false;
        expect(p, T_COLON);
        if (!parse_value(p, &m.value)) return false;

        arena_da_append(&p->a, &out->object, m);

    } while (accept(p, T_COMMA));

    if (!expect(p, T_RIGHT_BRACE)) return false;
    return true;
}

bool parse_value(Parser *p, JsonNode *out) {
    TokenType t = peekToken(p).type;

    switch (t) {
        case T_LEFT_BRACE:
            out->type = JSON_OBJECT;
            if(!parse_object(p, out)) return false;
        break;
        case T_LEFT_BRACKET:
            out->type = JSON_ARRAY;
            if(!parse_array(p, &out->array)) return false;
        break;
        case T_STRING:
            out->type = JSON_STRING;
            if(!parse_string(p, &out->string)) return false;
        break;
        case T_NUMBER:
            out->type = JSON_NUMBER;
            if (!parse_number(p, &out->number)) return false;
        break;
        case T_TRUE:
        case T_FALSE:
            out->type = JSON_BOOl;
            if (!parse_bool(p, &out->boolean)) return false;
        break;
        case T_NULL:
            out->type = JSON_NULL;
        break;
        default:
            printf("unknown symbol");
            return false;
    }

    return true;
}

int main() {
    Arena a = {0};
    Parser p = {
        .a = a,
        .pos = 0,
        .tokens = (TokenArray) {
            .items = (Token[5]){
                { .type = T_LEFT_BRACE },
                { .type = T_STRING, .string_val = "key" },
                { .type = T_COLON, },
                { .type = T_NUMBER, .number_val = 5 },
                { .type = T_RIGHT_BRACE },
            },
            .capacity = 5,
            .count = 5
        }
    };

    JsonNode root = {0};

    parse_value(&p, &root);

    arena_free(&a);
}
