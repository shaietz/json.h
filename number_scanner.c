#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum JsonNumberError JsonNumberError;
enum JsonNumberError {
    NONE,
    LEADING_ZERO,
    MISSING_FRACTION,
    MISSING_EXPONENT,
    MISSING_INTEGER,
};

bool is_delimiter(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\0';
}

bool is_valid_json_number(const char *input) {
    if ((*input) == '-') {
        input++;
        if (is_delimiter(*input)) return false;
    } 

    if ((*input) == '0') {
        input++;
        if (is_delimiter(*input)) return true;

        if (*input != '.') {
            return false;
        }
        input++;
        if (!isdigit(*input)) return false;
    }

    while (isdigit(*input)) input++;

    if ((*input) == '.') {
        input++;
        if (!isdigit(*input)) return false;
        while (isdigit(*input)) input++;
    }

    if ((*input) == 'e' || (*input) == 'E') {
        input++;
        if ((*input) == '-' || (*input) == '+') input++;

        if (!isdigit(*input)) return false;
        while (isdigit(*input)) input++;
    }

    return is_delimiter(*input);
}

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)

int main() {
    CHECK(is_valid_json_number("123"));
    CHECK(is_valid_json_number("-123"));
    CHECK(is_valid_json_number("0"));
    CHECK(is_valid_json_number("1.23"));
    CHECK(is_valid_json_number("1e10"));
    CHECK(is_valid_json_number("1e+10"));

    CHECK(is_valid_json_number("0123") == false);
    CHECK(!is_valid_json_number("1."));
    CHECK(!is_valid_json_number("1e"));
    CHECK(!is_valid_json_number("-"));
    
    puts("all tests passed");

    return 0;
}