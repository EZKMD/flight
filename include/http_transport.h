#ifndef HTTP_TRANSPORT_H
#define HTTP_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include "provider_result.h"

typedef struct {
    long status_code;
    char *body;
    size_t body_size;
    int retry_after_seconds;
} HttpResponse;

bool http_transport_init(void);
void http_transport_cleanup(void);
ProviderResult http_get(const char *url, long timeout_ms, HttpResponse *response);
void http_response_free(HttpResponse *response);
char *http_url_encode(const char *value);
void http_url_encoded_free(char *value);

#endif
