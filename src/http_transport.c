#include "http_transport.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct { char *data; size_t size; } BodyBuffer;
typedef struct { int retry_after_seconds; } HeaderData;

static ProviderResult result(ProviderStatus status, const char *message)
{
    ProviderResult value = { .status = status, .retry_after_seconds = 0 };
    (void)snprintf(value.message, sizeof(value.message), "%s", message);
    return value;
}

static size_t receive_body(char *contents, size_t size, size_t count, void *user_data)
{
    BodyBuffer *buffer = user_data;
    size_t bytes = size * count;
    char *expanded;
    if (bytes > SIZE_MAX - buffer->size - 1) return 0;
    expanded = realloc(buffer->data, buffer->size + bytes + 1);
    if (expanded == NULL) return 0;
    buffer->data = expanded;
    (void)memcpy(buffer->data + buffer->size, contents, bytes);
    buffer->size += bytes;
    buffer->data[buffer->size] = '\0';
    return bytes;
}

static size_t receive_header(char *contents, size_t size, size_t count, void *user_data)
{
    HeaderData *headers = user_data;
    size_t bytes = size * count;
    const char *retry_header = "Retry-After:";
    const char *opensky_header = "X-Rate-Limit-Retry-After-Seconds:";
    const char *value = NULL;
    size_t retry_length = strlen(retry_header);
    size_t opensky_length = strlen(opensky_header);
    if (bytes > retry_length && strncasecmp(contents, retry_header, retry_length) == 0)
        value = contents + retry_length;
    else if (bytes > opensky_length &&
             strncasecmp(contents, opensky_header, opensky_length) == 0)
        value = contents + opensky_length;
    if (value != NULL) {
        long seconds = strtol(value, NULL, 10);
        if (seconds > 0 && seconds <= 86400) headers->retry_after_seconds = (int)seconds;
    }
    return bytes;
}

bool http_transport_init(void) { return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK; }
void http_transport_cleanup(void) { curl_global_cleanup(); }

ProviderResult http_get(const char *url, long timeout_ms, HttpResponse *response)
{
    CURL *handle;
    CURLcode code;
    BodyBuffer body = { NULL, 0 };
    HeaderData headers = { 0 };
    response->status_code = 0;
    response->body = NULL;
    response->body_size = 0;
    response->retry_after_seconds = 0;
    handle = curl_easy_init();
    if (handle == NULL) return result(PROVIDER_UNAVAILABLE, "HTTP initialization failed");
    (void)curl_easy_setopt(handle, CURLOPT_URL, url);
    (void)curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, timeout_ms);
    (void)curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms / 2L);
    (void)curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    (void)curl_easy_setopt(handle, CURLOPT_USERAGENT, "flight/0.2");
    (void)curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, receive_body);
    (void)curl_easy_setopt(handle, CURLOPT_WRITEDATA, &body);
    (void)curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, receive_header);
    (void)curl_easy_setopt(handle, CURLOPT_HEADERDATA, &headers);
    code = curl_easy_perform(handle);
    (void)curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response->status_code);
    curl_easy_cleanup(handle);
    if (code != CURLE_OK) {
        free(body.data);
        if (code == CURLE_OPERATION_TIMEDOUT) return result(PROVIDER_TIMEOUT, "request timed out");
        return result(PROVIDER_UNAVAILABLE, curl_easy_strerror(code));
    }
    response->body = body.data;
    response->body_size = body.size;
    response->retry_after_seconds = headers.retry_after_seconds;
    if (response->status_code == 401 || response->status_code == 403)
        return result(PROVIDER_AUTH_ERROR, "provider rejected credentials");
    if (response->status_code == 429) {
        ProviderResult limited = result(PROVIDER_RATE_LIMITED, "provider rate limit reached");
        limited.retry_after_seconds = response->retry_after_seconds;
        return limited;
    }
    if (response->status_code < 200 || response->status_code >= 300)
        return result(PROVIDER_UNAVAILABLE, "provider returned an HTTP error");
    return result(PROVIDER_OK, "");
}

void http_response_free(HttpResponse *response)
{
    free(response->body);
    response->body = NULL;
    response->body_size = 0;
}

char *http_url_encode(const char *value)
{
    CURL *handle = curl_easy_init();
    char *encoded;
    char *copy;
    if (handle == NULL) return NULL;
    encoded = curl_easy_escape(handle, value, 0);
    copy = encoded != NULL ? strdup(encoded) : NULL;
    curl_free(encoded);
    curl_easy_cleanup(handle);
    return copy;
}

void http_url_encoded_free(char *value) { free(value); }
