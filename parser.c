#include "arena.h"
#include "tokenizer.c"
#include <stdio.h>

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} String_Builder;

typedef enum {
    JSON_STRING,
    JSON_NUMBER,
    JSON_BOOL,
    JSON_ARRAY,
    JSON_OBJECT,
    JSON_NULL,
} JsonType;

typedef struct JsonNode JsonNode;

typedef struct JsonArray JsonArray;
struct JsonArray {
    JsonNode *items;
    size_t count;
    size_t capacity;
};

struct JsonNode {
    JsonType type;
    
    union {
        char *string;
        double number;
        bool boolean;

        struct {
            JsonNode *items;
            size_t count;
        } array;

        struct {
            char **keys;
            JsonNode *values;
            size_t count;
        } object;
    };
};

void error(const char msg[]) {
    printf(msg);
    printf("\n");
}

typedef struct Parser Parser;
struct Parser {
    TokenArray tokens;
    size_t pos;
};

void consumeToken(Parser *p, JsonNode *node) {
    Token t = p->tokens.items[p->pos];

    switch (t.type) {
        case T_STRING:
            node->type = JSON_STRING;
            node->string = t.string_val;
        break;
        case T_NUMBER:
            node->type = JSON_NUMBER;
            node->number = t.number_val;
        break;
        case T_TRUE:
            node->type = JSON_BOOL;
            node->boolean = true;
        break;
        case T_FALSE:
            node->type = JSON_BOOL;
            node->boolean = false;
        break;
        case T_NULL:
            node->type = JSON_NULL;
        break;
        default:
        break;
    }

    p->pos++;
}

extern TokenType peekToken(Parser *p) {
    return (p->tokens.items[p->pos]).type;
}

bool accept(Parser *p, TokenType t, JsonNode *node) {
    if (peekToken(p) == t) {
        consumeToken(p, node);
        return true;
    }
    return false;
}

bool expect(Parser *p, TokenType t, JsonNode *node) {
    if (accept(p, t, node)) {
        return true;
    }
    error("expect: unexpected symbol");
    return false;
}

void key(Parser *p, JsonNode *node) {
    expect(p, T_STRING, node);
    expect(p, T_COLON, node);
}

void value(Parser *p, JsonNode *node) {
    if (accept(p, T_STRING, node) || accept(p, T_NUMBER, node) || accept(p, T_TRUE, node) || accept(p, T_FALSE, node) || accept(p, T_NULL, node)) {
        return;
    }

    if (accept(p, T_LEFT_BRACE, node)) {
        do {
            key(p, node);
            value(p, node);
        } while (accept(p, T_COMMA, node));
        expect(p, T_RIGHT_BRACE, node);
        return;
    } else if (accept(p, T_LEFT_BRACKET, node)) {
        do {
            value(p, node);
        } while (accept(p, T_COMMA, node));
        expect(p, T_RIGHT_BRACKET, node);
        return;
    }

    error("error: unexpected symbol");
}

int main() {
    Arena a = {0};
    String_Builder root_keys = {0};
    JsonArray root_values = {0};

    JsonNode root = {
        .type = JSON_OBJECT,
        .object = {
            .keys = &root_keys,
            .values = &root_values,
        }
    };



    Parser p = {
        .pos = 0,
        .tokens = (TokenArray) {
            .items = (Token[5]){
                { .type = T_LEFT_BRACE },
                { .type = T_STRING, .string_val = "key" },
                { .type = T_COLON, },
                { .type = T_STRING, .string_val = "value" },
                { .type = T_RIGHT_BRACE },
            },
            .capacity = 5,
            .count = 5
        }
    };

    value(&p, &root);
}
