#ifndef FLIGHT_JSON_H
#define FLIGHT_JSON_H

#include <stdbool.h>
#include <stddef.h>

typedef enum { JSON_UNDEFINED, JSON_OBJECT, JSON_ARRAY, JSON_STRING, JSON_PRIMITIVE } JsonType;

typedef struct {
    JsonType type;
    int start;
    int end;
    int parent;
} JsonToken;

typedef struct {
    const char *text;
    JsonToken *tokens;
    int token_count;
    int token_capacity;
} JsonDocument;

bool json_parse(JsonDocument *document, const char *text, JsonToken *tokens, int capacity);
int json_root(const JsonDocument *document);
int json_object_get(const JsonDocument *document, int object, const char *key);
int json_array_get(const JsonDocument *document, int array, int element);
bool json_is_null(const JsonDocument *document, int token);
bool json_string(const JsonDocument *document, int token, char *output, size_t capacity);
bool json_double(const JsonDocument *document, int token, double *value);
bool json_long(const JsonDocument *document, int token, long *value);
bool json_bool(const JsonDocument *document, int token, bool *value);

#endif
