#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

typedef enum {
    STRING,
    BOOL,
    NUMBER,
    // ARRAY,
    OBJECT,
    NULLTYPE
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

void skipSpaces(const char** text) {
    while (**text == ' ' || ** text == '\n' || **text == '\t') {
        (*text)++;
    }
}

char* readString(const char** text) {
    if(**text != '"') return NULL;
    (*text)++;

    const char* start = *text;
    while (**text != '"' && **text != '\0') {
        (*text)++;
    }

    int length = *text - start;
    char* result = (char*)malloc(length + 1);
    strncpy(result, start, length);
    result[length] = '\0';

    if (**text == '"') (*text)++;
    return result;
}

double readNumber(const char** text) {
    char* end;
    double num = strtod(*text, &end);
    *text = end;
    return num;
}

int readBool(const char** text) {
    if (strncmp(*text, "true", 4) == 0) {
        *text += 4;
        return 1;
    } else if (strncmp(*text, "false", 5) == 0) {
        *text += 5;
        return 0;
    }
    return 0;
}

JsonNode* readNode(const char** text);

JsonNode* readObject(const char** text) {
    JsonNode* obj = (JsonNode*)malloc(sizeof(JsonNode));
    obj->type = OBJECT;
    obj->objectValue.entries = malloc(sizeof(JsonEntry) * 20);
    obj->objectValue.count = 0;

    (*text)++;
    skipSpaces(text);

    while (**text != '}' && **text != '\0') {
        skipSpaces(text);
        char* key = readString(text);
        skipSpaces(text);
        if (**text == ':') (*text)++;
        skipSpaces(text);
        JsonNode* node = readNode(text);

        obj->objectValue.entries[obj->objectValue.count].key = key;
        obj->objectValue.entries[obj->objectValue.count].value = node;
        obj->objectValue.count++;

        skipSpaces(text);
        if (**text == ',') {
            (*text)++;
            skipSpaces(text);
        }
    }

    if (**text == '}') (*text)++;
    return obj;
}

JsonNode* readNode(const char** text) {
    skipSpaces(text);

    if (**text == '"') {
        JsonNode* node = malloc(sizeof(JsonNode));
        node->type = STRING;
        node->stringValue = readString(text);
        return node;
    } else if (isdigit(**text) || **text == '-') {
        JsonNode* node = malloc(sizeof(JsonNode));
        node->type = NUMBER;
        node->numberValue = readNumber(text);
        return node;
    } else if (strncmp(*text, "true", 4) == 0 || strncmp(*text, "false", 5) == 0) {
        JsonNode* node = malloc(sizeof(JsonNode));
        node->type = BOOL;
        node->boolValue = readBool(text);
        return node;
    } else if (strncmp(*text, "null", 4) == 0) {
        JsonNode* node = malloc(sizeof(JsonNode));
        node->type = NULLTYPE;
        *text += 4;
        return node;
    } else if (**text == '{') {
        return readObject(text);
    }

    return NULL;
}

char* loadFile(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char* data = malloc(size + 1);
    fread(data, 1, size, f);
    data[size] = '\0';
    fclose(f);
    return data;
}

JsonNode* getValue(JsonNode* root, const char* path) {
    if (root == NULL || root->type != OBJECT) return NULL;

    char* pathCopy = strdup(path);
    char* token = strtok(pathCopy, ".");
    JsonNode* current = root;

    while (token != NULL && current->type == OBJECT) {
        int found = 0;
        for (int i = 0; i < current->objectValue.count; i++) {
            if (strcmp(current->objectValue.entries[i].key, token) == 0) {
                current = current->objectValue.entries[i].value;
                found = 1;
                break;
            }
        }
        if (!found) {
            free(pathCopy);
            return NULL;
        }
        token = strtok(NULL, ".");
    }

    free(pathCopy);
    return current;
}

void printValue(JsonNode* value) {
    if (!value) {
        printf("Not found\n");
        return;
    }

    if (value->type == STRING) {
        printf("String: %s\n", value->stringValue);
    } else if (value->type == NUMBER) {
        printf("Number: %.2f\n", value->numberValue);
    } else if (value->type == BOOL) {
        printf("Boolean: %s\n", value->boolValue ? "true" : "false");
    } else if (value->type == NULLTYPE) {
        printf("Null\n");
    } else {
        printf("Object {...}\n");
    }
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

    char* jsonText = loadFile("data.json");
    if (!jsonText) {
        printf("file not found\n");
        return 1;
    }
    
    const char* p = jsonText;
    JsonNode* root = readNode(&p);

    printJsonNode(root);


    printf("Looking for details.name:\n");
    JsonNode* name = getValue(root, "details.name");
    printValue(name);

    printf("Looking for details.age:\n");
    JsonNode* age = getValue(root, "details.age");
    printValue(age);

    printf("Looking for active:\n");
    JsonNode* active = getValue(root, "active");
    printValue(active);

    free(jsonText);
    return 0;
}