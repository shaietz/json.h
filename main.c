#include "jp.h"

char* loadFile(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char* data = malloc(size + 1);
    size_t n = fread(data, 1, size, f);
    data[n] = '\0';
    fclose(f);
    return data;
}

void check(const char *input, const char *expected) {
    Arena a = {0};
    JsonNode root;
    JsonError err = {0};

    if(!jp_parse(&a, input, &root, &err)) {
        fprintf(stderr, "FAIL: %s\n parse error at byte %zu: %s\n", input, err.pos, err.msg); 
        exit(1);
    };

    char *buf; 
    size_t len;
    FILE *m = open_memstream(&buf, &len);
    jp_print(m, &root);
    fclose(m);

    if (strcmp(buf, expected) != 0) {
        fprintf(stderr, "FAIL: %s\n  expected:   %s\n  actual:   %s\n", input, expected, buf); 
        exit(1);
    }

    free(buf);
    arena_free(&a);
}

void check_fails(const char *input) {
    Arena a = {0};
    JsonNode root;
    JsonError err = (JsonError){0};

    if(jp_parse(&a, input, &root, &err)) {
        fprintf(stderr, "FAIL: %s\n  expected a parse error, but it parsed\n", input);
        exit(1);
    }; 

    arena_free(&a);
}

int main() {
    check("[1, true, null]",  "[1,true,null]");
    check("{\"a\" : {}}",     "{\"a\":{}}");

    check_fails("[1,]");
    check_fails("{\"a\" 1}");
    check_fails("\"unterminated");

    printf("ALL CHECKS PASS!\n");
    return 0;
}