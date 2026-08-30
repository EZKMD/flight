#include "json.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int add_token(JsonDocument *document, JsonType type, int start, int parent)
{
    JsonToken *token;
    if (document->token_count >= document->token_capacity) return -1;
    token = &document->tokens[document->token_count];
    token->type = type;
    token->start = start;
    token->end = -1;
    token->parent = parent;
    return document->token_count++;
}

bool json_parse(JsonDocument *document, const char *text, JsonToken *tokens, int capacity)
{
    int parent = -1;
    int index;
    document->text = text;
    document->tokens = tokens;
    document->token_count = 0;
    document->token_capacity = capacity;
    for (index = 0; text[index] != '\0'; index++) {
        char character = text[index];
        if (character == '{' || character == '[') {
            int token = add_token(document, character == '{' ? JSON_OBJECT : JSON_ARRAY,
                                  index, parent);
            if (token < 0) return false;
            parent = token;
        } else if (character == '}' || character == ']') {
            JsonType expected = character == '}' ? JSON_OBJECT : JSON_ARRAY;
            if (parent < 0 || document->tokens[parent].type != expected) return false;
            document->tokens[parent].end = index + 1;
            parent = document->tokens[parent].parent;
        } else if (character == '"') {
            int start = ++index;
            int token;
            while (text[index] != '\0' && text[index] != '"') {
                if (text[index] == '\\' && text[index + 1] != '\0') index++;
                index++;
            }
            if (text[index] != '"') return false;
            token = add_token(document, JSON_STRING, start, parent);
            if (token < 0) return false;
            document->tokens[token].end = index;
        } else if (character == ' ' || character == '\t' || character == '\r' ||
                   character == '\n' || character == ':' || character == ',') {
            continue;
        } else {
            int start = index;
            int token;
            while (text[index] != '\0' && text[index] != ',' && text[index] != ']' &&
                   text[index] != '}' && text[index] != ' ' && text[index] != '\t' &&
                   text[index] != '\r' && text[index] != '\n') index++;
            token = add_token(document, JSON_PRIMITIVE, start, parent);
            if (token < 0) return false;
            document->tokens[token].end = index;
            index--;
        }
    }
    return parent == -1 && document->token_count > 0;
}

int json_root(const JsonDocument *document) { return document->token_count > 0 ? 0 : -1; }

static bool token_equals(const JsonDocument *document, int token, const char *value)
{
    int length;
    if (token < 0 || token >= document->token_count) return false;
    length = document->tokens[token].end - document->tokens[token].start;
    return length == (int)strlen(value) &&
           strncmp(document->text + document->tokens[token].start, value,
                   (size_t)length) == 0;
}

int json_object_get(const JsonDocument *document, int object, const char *key)
{
    int index;
    if (object < 0 || object >= document->token_count ||
        document->tokens[object].type != JSON_OBJECT) return -1;
    for (index = object + 1; index + 1 < document->token_count; index++) {
        if (document->tokens[index].parent == object &&
            document->tokens[index].type == JSON_STRING && token_equals(document, index, key))
            return index + 1;
    }
    return -1;
}

int json_array_get(const JsonDocument *document, int array, int element)
{
    int index;
    int found = 0;
    if (array < 0 || array >= document->token_count ||
        document->tokens[array].type != JSON_ARRAY) return -1;
    for (index = array + 1; index < document->token_count; index++) {
        if (document->tokens[index].parent == array) {
            if (found == element) return index;
            found++;
        }
    }
    return -1;
}

bool json_is_null(const JsonDocument *document, int token)
{
    return token_equals(document, token, "null");
}

bool json_string(const JsonDocument *document, int token, char *output, size_t capacity)
{
    int input;
    size_t used = 0;
    JsonToken value;
    if (capacity == 0 || token < 0 || token >= document->token_count ||
        document->tokens[token].type != JSON_STRING) return false;
    value = document->tokens[token];
    for (input = value.start; input < value.end && used + 1 < capacity; input++) {
        char character = document->text[input];
        if (character == '\\' && input + 1 < value.end) {
            character = document->text[++input];
            if (character == 'n') character = '\n';
            else if (character == 'r') character = '\r';
            else if (character == 't') character = '\t';
        }
        output[used++] = character;
    }
    output[used] = '\0';
    return true;
}

static bool primitive_copy(const JsonDocument *document, int token, char *buffer,
                           size_t capacity)
{
    int length;
    if (token < 0 || token >= document->token_count || capacity == 0) return false;
    length = document->tokens[token].end - document->tokens[token].start;
    if (length <= 0 || (size_t)length >= capacity) return false;
    (void)memcpy(buffer, document->text + document->tokens[token].start, (size_t)length);
    buffer[length] = '\0';
    return true;
}

bool json_double(const JsonDocument *document, int token, double *value)
{
    char buffer[64];
    char *end;
    if (!primitive_copy(document, token, buffer, sizeof(buffer)) || strcmp(buffer, "null") == 0)
        return false;
    errno = 0;
    *value = strtod(buffer, &end);
    return errno == 0 && *end == '\0';
}

bool json_long(const JsonDocument *document, int token, long *value)
{
    char buffer[64];
    char *end;
    if (!primitive_copy(document, token, buffer, sizeof(buffer)) || strcmp(buffer, "null") == 0)
        return false;
    errno = 0;
    *value = strtol(buffer, &end, 10);
    return errno == 0 && *end == '\0';
}

bool json_bool(const JsonDocument *document, int token, bool *value)
{
    if (token_equals(document, token, "true")) { *value = true; return true; }
    if (token_equals(document, token, "false")) { *value = false; return true; }
    return false;
}
