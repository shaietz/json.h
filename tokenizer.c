#define ARENA_IMPLEMENTATION
#include "arena.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

typedef enum TokenType TokenType;
enum TokenType {
    T_LEFT_BRACE,
    T_RIGHT_BRACE,
    T_LEFT_BRACKET,
    T_RIGHT_BRACKET,
    T_COLON,
    T_COMMA,
    T_STRING,
    T_NUMBER,
    T_TRUE,
    T_FALSE,
    T_NULL,
};

typedef struct Token Token;
struct Token {
    TokenType type;
    union {
        char *string_val;
        double number_val;
    };
};

typedef struct TokenArray TokenArray;
struct TokenArray {
    Token *items;
    size_t capacity;
    size_t count;
};

bool tokenize(Arena *a, char **input, TokenArray *tokens) {
    while (**input != '\0') {
        char c = (**input);

        if (c == '{') {
            arena_da_append(a, tokens, (Token){.type = T_LEFT_BRACE});
            (*input)++;
        } else if (c == '}') {
            arena_da_append(a, tokens, (Token){.type = T_RIGHT_BRACE});
            (*input)++;
        } else if (c == '[') {
            arena_da_append(a, tokens, (Token){.type = T_LEFT_BRACKET});
            (*input)++;
        } else if (c == ']'){
            arena_da_append(a, tokens, (Token){.type = T_RIGHT_BRACKET});
            (*input)++;
        } else if (c == ':'){
            arena_da_append(a, tokens, (Token){.type = T_COLON});
            (*input)++;
        } else if (c == ',') {
            arena_da_append(a, tokens, (Token){.type = T_COMMA});
            (*input)++;
        } else if (c == '"') {
            (*input)++;


            const char* start = *input;
            bool escape = false;

            while (!(!escape && **input == '"') && **input != '\0') {
                escape = (!escape && (**input) == '\\');
                (*input)++;
            }

            size_t length = (size_t)(*input - start);
            char* result = arena_alloc(a, length + 1);
            memcpy(result, start, length);
            result[length] = '\0';

            arena_da_append(a, tokens, ((Token){.type = T_STRING, .string_val = result}));
            if (**input != '"') return false;
            (*input)++;
        } else if (isdigit(**input) || **input == '-') {
            char* end;
            double num = strtod(*input, &end);
            *input = end;
            arena_da_append(a, tokens, ((Token){.type = T_NUMBER, .number_val = num}));
        } else if (strncmp(*input, "true", 4) == 0) {
            arena_da_append(a, tokens, ((Token){.type = T_TRUE}));
            *input += 4;
        } else if (strncmp(*input, "false", 5) == 0) {
            arena_da_append(a, tokens, ((Token){.type = T_FALSE}));
            *input += 5;
        } else if (strncmp(*input, "null", 4) == 0) {
            arena_da_append(a, tokens, ((Token){.type = T_NULL}));
            *input += 4;
        } else if (**input == ' ' || **input == '\n' || **input == '\t') {
            (*input)++;
        } else {
            return false;
        }
    }

    return true;
}

// int main() {
//     Arena a = {0};

//     char* input = "{   \"key\":      \"v\\\"alue\"       }";
//     TokenArray tokens = {0};

//     bool ok = tokenize(&a, &input, &tokens);
     
//     if (!ok) {
//         printf("FAILED\n");
//         return 1;
//     }

//     for (size_t i = 0; i < tokens.count; i++) {
//         printf("Token: %d\n", tokens.items[i].type);
//     }

//     arena_free(&a);

//     return 0;
// }