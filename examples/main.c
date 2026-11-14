#include "json.h"

int main(void) {
    Arena arena = arena_make(MB(64));

    Str8 text = S8("{\"user\": {\"name\": \"shai\", \"tags\": [\"c\", \"json\"]}}");
    JsonNode root;
    JsonError err;
    if (!json_parse(&arena, text, &root, &err)) {
        printf("error at %td:%td: %s\n", err.line, err.col, err.msg);
        return 1;
    }

    JsonNode *name = json_get(json_get(&root, S8("user")), S8("name"));
    if (name && name->type == JSON_STRING) {
        printf("name: " STR8_FMT "\n", str8_varg(name->string));
    }

    JsonNode *tag = json_at(json_get(json_get(&root, S8("user")), S8("tags")), 1);
    if (tag) printf("second tag: " STR8_FMT "\n", str8_varg(tag->string));

    Str8 out = json_serialize(&arena, &root);
    printf(STR8_FMT "\n", str8_varg(out));
}