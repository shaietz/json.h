#include <stdio.h>

typedef enum {
    STRING,
    BOOL,
    NUMBER,
    // ARRAY,
    OBJECT,
    TYPE_NULL

} JsonType;

struct JsonNode;

typedef struct {
    char* key;
    struct JsonNode* value;
} JsonEntry;

typedef struct JsonNode {
    JsonType type;
    union {
        char* stringValue;
        double numberValue;
        int boolValue;
        struct {
            JsonEntry* entries;
            int count;
        } objectValue;
    };
} JsonNode;

typedef enum {
    LEFT_BRACE,
    RIGHT_BRACE,
    LEFT_BRACKET,
    RIGHT_BRACKET,
    COLON,
    COMMA,
    STRING,
    NUMBER,
    TRUE,
    FALSE,
    NULL_TOKEN,
} TokenType;

typedef struct {
    TokenType type;
    char* value;
} Token;

Token* lexer(char* input) {
        
}

void printJsonNode(JsonNode* node) {
    
    switch (node->type)
    {
    case STRING:
        printf("\"%s\"", node->stringValue);
        break;
    
    case BOOL:
        printf("%s", node->boolValue ? "true" : "false");
        break;

    case NUMBER:
        printf("%f", node->numberValue);
        break;

    case OBJECT:
        printf("{");
        for (int i = 0; i < node->objectValue.count; i++) {
            JsonEntry entry = node->objectValue.entries[i];
            printf("\"%s\": ", entry.key);
            printJsonNode(entry.value);
            if (i < node->objectValue.count - 1) {
                printf(", ");
            }
        }
        printf("}\n");
        break;
    
    default:
        break;
    }
}

int main() {
    JsonNode innerNode1 = {
        .type = STRING, 
        .stringValue = "value" 
    };

    JsonNode innerNode2 = {
        .type = BOOL, 
        .boolValue = 1
    };

    JsonEntry entries[2] = { {.key = "key1", .value = &innerNode1},  {.key = "key2", .value = &innerNode2}};

    JsonNode node = {
        .type = OBJECT,
        .objectValue = {
            .count = 2,
            .entries = entries
        }
    };

    printJsonNode(&node);

    return 0;
}