#include "jp.h"

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

int main() {
    const char* jsonText = loadFile("data.json");
    if (!jsonText) {
        printf("file not found\n");
        return 1;
    }
    
    Arena a = {0};
    JsonNode root;
    JsonError err;

    if (!jp_parse(&a, jsonText, &root, &err)) {
        fprintf(stderr, "parse error at byte %zu: %s\n", err.pos, err.msg);
    }

    arena_free(&a);
    return 0;
}