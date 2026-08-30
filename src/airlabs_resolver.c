#include "airlabs_resolver.h"

#include "airport_reference.h"
#include "flight_candidate.h"
#include "flight_designator.h"
#include "http_transport.h"
#include "json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_TOKEN_LIMIT 4096

static ProviderResult make_result(ProviderStatus status, const char *message)
{
    ProviderResult result = { .status = status, .retry_after_seconds = 0 };
    (void)snprintf(result.message, sizeof(result.message), "%s", message);
    return result;
}

static void get_string(const JsonDocument *document, int object, const char *key,
                       char *output, size_t capacity)
{
    (void)json_string(document, json_object_get(document, object, key), output, capacity);
}

static OptionalTime get_time(const JsonDocument *document, int object, const char *key)
{
    long value;
    if (json_long(document, json_object_get(document, object, key), &value))
        return (OptionalTime){ true, (time_t)value };
    return (OptionalTime){ false, 0 };
}

static OptionalInt get_int(const JsonDocument *document, int object, const char *key)
{
    long value;
    if (json_long(document, json_object_get(document, object, key), &value))
        return (OptionalInt){ true, (int)value };
    return (OptionalInt){ false, 0 };
}

static void time_display(const char *date_time, char output[8])
{
    if (strlen(date_time) >= 16) (void)snprintf(output, 8, "%.5s", date_time + 11);
}

static bool active_status(const char *status)
{
    return strcmp(status, "active") == 0 || strcmp(status, "en-route") == 0 ||
           strcmp(status, "en_route") == 0;
}

static ProviderResult api_error(const JsonDocument *document, int root)
{
    int error = json_object_get(document, root, "error");
    char code[48] = "";
    char message[128] = "AirLabs returned an API error";
    if (error < 0) return make_result(PROVIDER_OK, "");
    get_string(document, error, "code", code, sizeof(code));
    get_string(document, error, "message", message, sizeof(message));
    if (strcmp(code, "not_found") == 0) return make_result(PROVIDER_NOT_FOUND, message);
    if (strstr(message, "wrong format") != NULL || strstr(code, "format") != NULL)
        return make_result(PROVIDER_REJECTED_DESIGNATOR,
                           "provider rejected the commercial designator format");
    if (strstr(code, "access") != NULL || strstr(code, "key") != NULL)
        return make_result(PROVIDER_AUTH_ERROR, "AirLabs rejected authentication");
    if (strstr(code, "limit") != NULL) return make_result(PROVIDER_RATE_LIMITED, message);
    return make_result(PROVIDER_UNAVAILABLE, message);
}

static bool parse_candidate(const JsonDocument *document, int object,
                            const FlightResolveRequest *request, FlightCandidate *candidate)
{
    ResolvedFlight *flight = &candidate->flight;
    ResolvedFlightLeg *leg = &flight->selected_leg;
    char departure_text[32] = "";
    char arrival_text[32] = "";
    AirportValidation origin_validation;
    AirportValidation destination_validation;
    long departure_epoch = 0;
    memset(candidate, 0, sizeof(*candidate));
    get_string(document, object, "flight_iata", flight->identity.flight_number,
               sizeof(flight->identity.flight_number));
    if (flight->identity.flight_number[0] == '\0' ||
        strcmp(flight->identity.flight_number, request->flight_number) != 0) return false;
    get_string(document, object, "airline_iata", flight->identity.airline_code,
               sizeof(flight->identity.airline_code));
    (void)snprintf(flight->identity.airline_name, sizeof(flight->identity.airline_name), "%s",
                   flight->identity.airline_code);
    get_string(document, object, "model", leg->aircraft.model, sizeof(leg->aircraft.model));
    if (leg->aircraft.model[0] == '\0')
        get_string(document, object, "aircraft_icao", leg->aircraft.model,
                   sizeof(leg->aircraft.model));
    get_string(document, object, "reg_number", leg->aircraft.registration,
               sizeof(leg->aircraft.registration));
    get_string(document, object, "hex", leg->aircraft.icao24, sizeof(leg->aircraft.icao24));
    {
        size_t index;
        for (index = 0; leg->aircraft.icao24[index] != '\0'; index++)
            leg->aircraft.icao24[index] =
                (char)tolower((unsigned char)leg->aircraft.icao24[index]);
    }
    get_string(document, object, "flight_icao", leg->aircraft.callsign,
               sizeof(leg->aircraft.callsign));
    get_string(document, object, "dep_iata", leg->origin.iata, sizeof(leg->origin.iata));
    get_string(document, object, "dep_icao", leg->origin.icao, sizeof(leg->origin.icao));
    get_string(document, object, "arr_iata", leg->destination.iata,
               sizeof(leg->destination.iata));
    get_string(document, object, "arr_icao", leg->destination.icao,
               sizeof(leg->destination.icao));
    get_string(document, object, "dep_time", departure_text, sizeof(departure_text));
    get_string(document, object, "arr_time", arrival_text, sizeof(arrival_text));
    time_display(departure_text, leg->timing.departure_display);
    time_display(arrival_text, leg->timing.arrival_display);
    if (strlen(departure_text) >= 10)
        (void)snprintf(leg->departure_date, sizeof(leg->departure_date), "%.10s", departure_text);
    leg->timing.scheduled_departure = get_time(document, object, "dep_time_ts");
    leg->timing.estimated_departure = get_time(document, object, "dep_estimated_ts");
    leg->timing.actual_departure = get_time(document, object, "dep_actual_ts");
    leg->timing.scheduled_arrival = get_time(document, object, "arr_time_ts");
    leg->timing.estimated_arrival = get_time(document, object, "arr_estimated_ts");
    leg->timing.actual_arrival = get_time(document, object, "arr_actual_ts");
    leg->duration_minutes = get_int(document, object, "duration");
    leg->delay_minutes = get_int(document, object, "dep_delayed");
    leg->updated = get_time(document, object, "updated");
    get_string(document, object, "status", leg->provider_status, sizeof(leg->provider_status));
    origin_validation = airport_reference_normalize(&leg->origin);
    destination_validation = airport_reference_normalize(&leg->destination);
    candidate->airport_consistent = origin_validation != AIRPORT_MISMATCH &&
                                    destination_validation != AIRPORT_MISMATCH;
    if (leg->timing.scheduled_departure.available)
        departure_epoch = (long)leg->timing.scheduled_departure.value;
    (void)snprintf(leg->leg_id, sizeof(leg->leg_id), "%s-%s-%ld",
                   leg->origin.iata, leg->destination.iata, departure_epoch);
    (void)snprintf(flight->occurrence_id, sizeof(flight->occurrence_id), "%s-%ld",
                   flight->identity.flight_number, departure_epoch);
    flight->confidence = candidate->airport_consistent ? OCCURRENCE_CONFIRMED :
                         OCCURRENCE_INFERRED;
    return true;
}

static bool candidate_exists(const FlightCandidateSet *set, const FlightCandidate *candidate)
{
    size_t index;
    for (index = 0; index < set->count; index++)
        if (strcmp(set->items[index].flight.selected_leg.leg_id,
                   candidate->flight.selected_leg.leg_id) == 0) return true;
    return false;
}

static ProviderResult fetch_json(const char *url, JsonDocument *document, JsonToken *tokens,
                                 HttpResponse *response)
{
    ProviderResult transport = http_get(url, 8000L, response);
    if (transport.status != PROVIDER_OK) return transport;
    if (response->body == NULL || !json_parse(document, response->body, tokens, JSON_TOKEN_LIMIT))
        return make_result(PROVIDER_INVALID_RESPONSE, "AirLabs returned invalid JSON");
    return make_result(PROVIDER_OK, "");
}

static ProviderResult collect_flight(AirLabsResolverContext *context,
                                     const FlightResolveRequest *request,
                                     FlightCandidateSet *set, bool *needs_schedules)
{
    HttpResponse response;
    ProviderResult result;
    JsonToken tokens[JSON_TOKEN_LIMIT];
    JsonDocument document;
    char *flight = http_url_encode(request->flight_number);
    char *key = http_url_encode(context->api_key);
    char url[512];
    int root;
    int wrapped;
    if (flight == NULL || key == NULL) {
        http_url_encoded_free(flight);
        http_url_encoded_free(key);
        return make_result(PROVIDER_UNAVAILABLE, "could not encode AirLabs request");
    }
    (void)snprintf(url, sizeof(url),
                   "https://airlabs.co/api/v9/flight?flight_iata=%s&api_key=%s", flight, key);
    http_url_encoded_free(flight);
    http_url_encoded_free(key);
    result = fetch_json(url, &document, tokens, &response);
    if (result.status != PROVIDER_OK) { http_response_free(&response); return result; }
    root = json_root(&document);
    result = api_error(&document, root);
    if (result.status != PROVIDER_OK) {
        *needs_schedules = result.status == PROVIDER_NOT_FOUND;
        http_response_free(&response);
        return result;
    }
    wrapped = json_object_get(&document, root, "response");
    if (wrapped >= 0) root = wrapped;
    if (set->count < FLIGHT_CANDIDATE_LIMIT &&
        parse_candidate(&document, root, request, &set->items[set->count])) {
        FlightCandidate *candidate = &set->items[set->count++];
        ResolvedFlightLeg *leg = &candidate->flight.selected_leg;
        *needs_schedules = !candidate->airport_consistent ||
                           !active_status(leg->provider_status) ||
                           (request->date != NULL && request->date[0] != '\0' &&
                            strcmp(request->date, leg->departure_date) != 0);
    } else *needs_schedules = true;
    http_response_free(&response);
    return make_result(PROVIDER_OK, "");
}

static ProviderResult collect_schedules(AirLabsResolverContext *context,
                                        const FlightResolveRequest *request,
                                        FlightCandidateSet *set)
{
    HttpResponse response;
    ProviderResult result;
    JsonToken tokens[JSON_TOKEN_LIMIT];
    JsonDocument document;
    char *flight = http_url_encode(request->flight_number);
    char *key = http_url_encode(context->api_key);
    char url[512];
    int root;
    int wrapped;
    int index;
    if (flight == NULL || key == NULL) {
        http_url_encoded_free(flight);
        http_url_encoded_free(key);
        return make_result(PROVIDER_UNAVAILABLE, "could not encode AirLabs request");
    }
    (void)snprintf(url, sizeof(url),
                   "https://airlabs.co/api/v9/schedules?flight_iata=%s&limit=50&api_key=%s",
                   flight, key);
    http_url_encoded_free(flight);
    http_url_encoded_free(key);
    result = fetch_json(url, &document, tokens, &response);
    if (result.status != PROVIDER_OK) { http_response_free(&response); return result; }
    root = json_root(&document);
    result = api_error(&document, root);
    if (result.status != PROVIDER_OK) { http_response_free(&response); return result; }
    wrapped = json_object_get(&document, root, "response");
    if (wrapped >= 0) root = wrapped;
    for (index = 0; set->count < FLIGHT_CANDIDATE_LIMIT; index++) {
        int object = json_array_get(&document, root, index);
        if (object < 0) break;
        FlightCandidate parsed;
        if (parse_candidate(&document, object, request, &parsed) &&
            !candidate_exists(set, &parsed)) set->items[set->count++] = parsed;
    }
    http_response_free(&response);
    return make_result(PROVIDER_OK, "");
}

static ProviderResult resolve_airlabs(void *opaque, const FlightResolveRequest *request,
                                      ResolvedFlight *resolved)
{
    AirLabsResolverContext *context = opaque;
    FlightCandidateSet candidates = { .count = 0 };
    ProviderResult flight_result;
    ProviderResult schedules_result = make_result(PROVIDER_OK, "");
    ProviderResult selection;
    bool needs_schedules = false;
    if (flight_designator_classify(request->flight_number) ==
        DESIGNATOR_PLAUSIBLE_UNSUPPORTED)
        return make_result(PROVIDER_UNSUPPORTED_DESIGNATOR,
                           "three-character airline prefixes are not converted to IATA");
    if (flight_designator_classify(request->flight_number) == DESIGNATOR_MALFORMED)
        return make_result(PROVIDER_MALFORMED_DESIGNATOR,
                           "expected airline prefix followed by a flight number");
    if (context->api_key == NULL || context->api_key[0] == '\0')
        return make_result(PROVIDER_API_KEY_MISSING, "AIRLABS_API_KEY is not set");
    flight_result = collect_flight(context, request, &candidates, &needs_schedules);
    if (needs_schedules) schedules_result = collect_schedules(context, request, &candidates);
    if (candidates.count == 0) {
        if (schedules_result.status != PROVIDER_OK) return schedules_result;
        return flight_result.status != PROVIDER_OK ? flight_result :
               make_result(PROVIDER_NOT_FOUND, "flight occurrence not found");
    }
    selection = flight_candidate_select(&candidates, request, resolved);
    selection.candidate_count = (int)candidates.count;
    selection.selection_score = resolved->selection_score;
    (void)snprintf(selection.selection_reason, sizeof(selection.selection_reason), "%s",
                   resolved->selection_reason);
    return selection;
}

void airlabs_resolver_init(FlightResolver *resolver, AirLabsResolverContext *context,
                           const char *api_key)
{
    context->api_key = api_key;
    resolver->context = context;
    resolver->resolve = resolve_airlabs;
    resolver->name = "AIRLABS";
}
