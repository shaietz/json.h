#include "json.h"

int main(int argc, char **argv) {
    Arena a = arena_make(MB(64));
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    Str8 src = {a.beg, (isize)fread(a.beg, 1, (usize)(a.end - a.beg), f)};
    a.beg += src.len;
    fclose(f);
    JsonNode root;
    return json_parse(&a, src, &root, 0) ? 0 : 1;
}