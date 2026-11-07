
#include "tokenizer.c"
#include <stdio.h>


void error(const char msg[]) {
    printf(msg);
    printf("\n");
}

typedef struct Parser Parser;
struct Parser {
    TokenArray tokens;
    size_t pos;
};

void consumeToken(Parser *p) {
    p->pos++;
}

extern TokenType peekToken(Parser *p) {
    return (p->tokens.items[p->pos]).type;
}

bool accept(Parser *p, TokenType t) {
    if (peekToken(p) == t) {
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

void key(Parser *p) {
    expect(p, T_STRING);
    expect(p, T_COLON);
}

void value(Parser *p) {
    if (accept(p, T_STRING) || accept(p, T_NUMBER) || accept(p, T_TRUE) || accept(p, T_FALSE) || accept(p, T_NULL)) {
        return;
    }

    if (accept(p, T_LEFT_BRACE)) {
        do {
            key(p);
            value(p);
        } while (accept(p, T_COMMA));
        expect(p, T_RIGHT_BRACE);
        return;
    } else if (accept(p, T_LEFT_BRACKET)) {
        do {
            value(p);
        } while (accept(p, T_COMMA));
        expect(p, T_RIGHT_BRACKET);
        return;
    }

    error("error: unexpected symbol");
}

int main() {
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

    value(&p);
}
