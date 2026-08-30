#include "provider_result.h"

const char *provider_status_name(ProviderStatus status)
{
    switch (status) {
        case PROVIDER_OK: return "ok";
        case PROVIDER_NOT_FOUND: return "not found";
        case PROVIDER_UNAVAILABLE: return "unavailable";
        case PROVIDER_TIMEOUT: return "timeout";
        case PROVIDER_RATE_LIMITED: return "rate limited";
        case PROVIDER_AUTH_ERROR: return "authentication failed";
        case PROVIDER_API_KEY_MISSING: return "API key missing";
        case PROVIDER_INVALID_RESPONSE: return "invalid response";
        case PROVIDER_OCCURRENCE_AMBIGUOUS: return "occurrence ambiguous";
        case PROVIDER_PARTIAL_SUCCESS: return "partial success";
        case PROVIDER_TELEMETRY_STALE: return "telemetry stale";
        case PROVIDER_TELEMETRY_UNAVAILABLE: return "telemetry unavailable";
        case PROVIDER_STALE_CACHE: return "stale cache";
        case PROVIDER_MALFORMED_DESIGNATOR: return "malformed designator";
        case PROVIDER_UNSUPPORTED_DESIGNATOR: return "unsupported designator format";
        case PROVIDER_REJECTED_DESIGNATOR: return "provider rejected commercial designator";
    }
    return "unknown";
}
