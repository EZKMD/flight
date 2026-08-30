#ifndef PROVIDER_RESULT_H
#define PROVIDER_RESULT_H

typedef enum {
    PROVIDER_OK,
    PROVIDER_NOT_FOUND,
    PROVIDER_UNAVAILABLE,
    PROVIDER_TIMEOUT,
    PROVIDER_RATE_LIMITED,
    PROVIDER_AUTH_ERROR,
    PROVIDER_API_KEY_MISSING,
    PROVIDER_INVALID_RESPONSE,
    PROVIDER_OCCURRENCE_AMBIGUOUS,
    PROVIDER_PARTIAL_SUCCESS,
    PROVIDER_TELEMETRY_STALE,
    PROVIDER_TELEMETRY_UNAVAILABLE,
    PROVIDER_STALE_CACHE,
    PROVIDER_MALFORMED_DESIGNATOR,
    PROVIDER_UNSUPPORTED_DESIGNATOR,
    PROVIDER_REJECTED_DESIGNATOR
} ProviderStatus;

typedef struct {
    ProviderStatus status;
    int retry_after_seconds;
    char message[128];
    int candidate_count;
    int selection_score;
    char selection_reason[128];
} ProviderResult;

const char *provider_status_name(ProviderStatus status);

#endif
