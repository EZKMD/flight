#include "opensky_telemetry.h"

#include "http_transport.h"
#include "json.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define JSON_TOKEN_LIMIT 512

static ProviderResult make_result(ProviderStatus status, const char *message)
{
    ProviderResult result = { .status = status, .retry_after_seconds = 0 };
    (void)snprintf(result.message, sizeof(result.message), "%s", message);
    return result;
}

static OptionalDouble array_double(const JsonDocument *document, int array, int element)
{
    double value;
    if (json_double(document, json_array_get(document, array, element), &value))
        return (OptionalDouble){ true, value };
    return (OptionalDouble){ false, 0.0 };
}

static OptionalTime array_time(const JsonDocument *document, int array, int element)
{
    long value;
    if (json_long(document, json_array_get(document, array, element), &value))
        return (OptionalTime){ true, (time_t)value };
    return (OptionalTime){ false, 0 };
}

static void trim(char *text)
{
    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) text[--length] = '\0';
}

static ProviderResult fetch_opensky(void *opaque, const TelemetryRequest *request,
                                    TelemetrySnapshot *snapshot)
{
    HttpResponse response;
    ProviderResult transport;
    JsonToken tokens[JSON_TOKEN_LIMIT];
    JsonDocument document;
    char url[256];
    int states;
    int row;
    int token;
    bool on_ground;
    (void)opaque;
    memset(snapshot, 0, sizeof(*snapshot));
    if (request->icao24[0] == '\0')
        return make_result(PROVIDER_NOT_FOUND, "no ICAO24 available for telemetry lookup");
    (void)snprintf(url, sizeof(url),
                   "https://opensky-network.org/api/states/all?icao24=%s", request->icao24);
    transport = http_get(url, 8000L, &response);
    if (transport.status != PROVIDER_OK) {
        http_response_free(&response);
        return transport;
    }
    if (response.body == NULL || !json_parse(&document, response.body, tokens, JSON_TOKEN_LIMIT)) {
        http_response_free(&response);
        return make_result(PROVIDER_INVALID_RESPONSE, "OpenSky returned invalid JSON");
    }
    states = json_object_get(&document, json_root(&document), "states");
    row = json_array_get(&document, states, 0);
    if (row < 0) {
        http_response_free(&response);
        return make_result(PROVIDER_NOT_FOUND, "no current ADS-B position");
    }
    (void)json_string(&document, json_array_get(&document, row, 0), snapshot->icao24,
                      sizeof(snapshot->icao24));
    (void)json_string(&document, json_array_get(&document, row, 1), snapshot->callsign,
                      sizeof(snapshot->callsign));
    trim(snapshot->callsign);
    if (!telemetry_snapshot_matches(request, snapshot)) {
        http_response_free(&response);
        return make_result(PROVIDER_INVALID_RESPONSE, "OpenSky ICAO24 mismatch");
    }
    snapshot->time_position = array_time(&document, row, 3);
    snapshot->last_contact = array_time(&document, row, 4);
    snapshot->longitude = array_double(&document, row, 5);
    snapshot->latitude = array_double(&document, row, 6);
    snapshot->barometric_altitude_m = array_double(&document, row, 7);
    token = json_array_get(&document, row, 8);
    if (json_bool(&document, token, &on_ground)) snapshot->on_ground = (OptionalBool){ true, on_ground };
    snapshot->velocity_mps = array_double(&document, row, 9);
    snapshot->heading_degrees = array_double(&document, row, 10);
    snapshot->vertical_rate_mps = array_double(&document, row, 11);
    snapshot->geometric_altitude_m = array_double(&document, row, 13);
    snapshot->received_at = (OptionalTime){ true, time(NULL) };
    http_response_free(&response);
    return make_result(PROVIDER_OK, "");
}

void opensky_telemetry_init(TelemetryProvider *provider,
                            OpenSkyTelemetryContext *context)
{
    context->reserved = 0;
    provider->context = context;
    provider->fetch = fetch_opensky;
    provider->name = "OPENSKY";
}
