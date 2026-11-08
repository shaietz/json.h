#include <stdlib.h>
#include <stdbool.h>

typedef enum {
    JSON_NULL,
    JSON_BOOl,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonNode JsonNode;
typedef struct JsonArray JsonArray;
typedef struct JsonMember JsonMember;
typedef struct JsonObject JsonObject;

typedef struct JsonString {
    char *ptr;
    size_t len;
} JsonString;

struct JsonArray {
    JsonNode *items; 
    size_t count; 
    size_t capacity; 
};

struct JsonObject {
    JsonMember *items; 
    size_t count; 
    size_t capacity; 
};

struct JsonNode {
    JsonType type;
    union {
        bool boolean;
        double number;
        JsonString string;
        JsonArray array;
        JsonObject object;
    };
};

struct JsonMember {
    JsonString key;
    JsonNode value;
};
