#include <stdlib.h>
#include <stdbool.h>
#include <arena.h>

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
}

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
            String_Builder keys;
            JsonArray values;
            size_t count;
        } object;
    };
};

typedef struct JsonBuilder {
    JsonNode *root;

    char *key;
    JsonNode value;
} JsonBuilder;

// void jb_set_key(JsonBuilder *jb, char *key){
//     jb->key = key;
// }

// void jb_set_value(JsonBuilder *jb, char *value){
//     jb->value = value;
// }

// void jb_set_kv(JsonBuilder *jb, JsonNode *node){
//     Arena a = {0};
//     arena_sb_append_cstr(&a, &node->object.keys, jb->key);
//     arena_da_append(&a, &node->object.values, jb->value);
// }


