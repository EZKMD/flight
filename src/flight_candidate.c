#include "flight_candidate.h"

#include <stdio.h>
#include <string.h>

#define SCORE_REJECT (-1000000)
#define ACTIVE_SCORE 1200
#define SOON_SCORE 900
#define RECENT_LANDED_SCORE 500
#define DATE_MATCH_SCORE 2000

static OptionalTime departure_time(const FlightTiming *timing)
{
    if (timing->actual_departure.available) return timing->actual_departure;
    if (timing->estimated_departure.available) return timing->estimated_departure;
    return timing->scheduled_departure;
}

static OptionalTime arrival_time(const FlightTiming *timing)
{
    if (timing->actual_arrival.available) return timing->actual_arrival;
    if (timing->estimated_arrival.available) return timing->estimated_arrival;
    return timing->scheduled_arrival;
}

static bool active_status(const char *status)
{
    return strcmp(status, "active") == 0 || strcmp(status, "en-route") == 0 ||
           strcmp(status, "en_route") == 0;
}

int flight_candidate_score(FlightCandidate *candidate, const FlightResolveRequest *request)
{
    ResolvedFlightLeg *leg = &candidate->flight.selected_leg;
    OptionalTime departure = departure_time(&leg->timing);
    OptionalTime arrival = arrival_time(&leg->timing);
    long departure_delta = departure.available ? (long)(departure.value - request->now) : 0;
    long arrival_age = arrival.available ? (long)(request->now - arrival.value) : 0;
    int score = 0;
    candidate->reason[0] = '\0';
    if (!candidate->airport_consistent || leg->origin.iata[0] == '\0' ||
        leg->destination.iata[0] == '\0' || !departure.available || !arrival.available ||
        arrival.value <= departure.value) {
        (void)snprintf(candidate->reason, sizeof(candidate->reason), "invalid or inconsistent leg");
        candidate->score = SCORE_REJECT;
        return candidate->score;
    }
    if (request->date != NULL && request->date[0] != '\0') {
        if (strcmp(request->date, leg->departure_date) != 0) {
            (void)snprintf(candidate->reason, sizeof(candidate->reason), "requested date mismatch");
            candidate->score = SCORE_REJECT;
            return candidate->score;
        }
        score += DATE_MATCH_SCORE;
    }
    if (active_status(leg->provider_status) ||
        (departure.value <= request->now && arrival.value >= request->now)) {
        score += ACTIVE_SCORE;
        (void)snprintf(candidate->reason, sizeof(candidate->reason), "active occurrence");
    } else if (departure_delta >= 0) {
        long hours = departure_delta / 3600;
        score += SOON_SCORE - (int)(hours > 48 ? 480 : hours * 10);
        (void)snprintf(candidate->reason, sizeof(candidate->reason), "departs in %ld minutes",
                       departure_delta / 60);
    } else if (arrival_age >= 0) {
        long hours = arrival_age / 3600;
        score += RECENT_LANDED_SCORE - (int)(hours > 48 ? 480 : hours * 10);
        (void)snprintf(candidate->reason, sizeof(candidate->reason), "landed %ld minutes ago",
                       arrival_age / 60);
    }
    if (leg->aircraft.icao24[0] != '\0') score += 80;
    if (leg->aircraft.callsign[0] != '\0') score += 30;
    candidate->score = score;
    return score;
}

ProviderResult flight_candidate_select(FlightCandidateSet *set,
                                       const FlightResolveRequest *request,
                                       ResolvedFlight *selected)
{
    ProviderResult result = { .status = PROVIDER_NOT_FOUND, .retry_after_seconds = 0 };
    size_t index;
    int best_score = SCORE_REJECT;
    int second_score = SCORE_REJECT;
    size_t best = 0;
    for (index = 0; index < set->count; index++) {
        int score = flight_candidate_score(&set->items[index], request);
        if (score > best_score) {
            second_score = best_score;
            best_score = score;
            best = index;
        } else if (score > second_score) second_score = score;
    }
    if (best_score == SCORE_REJECT) {
        (void)snprintf(result.message, sizeof(result.message), "no valid occurrence candidates");
        return result;
    }
    *selected = set->items[best].flight;
    selected->candidate_count = (int)set->count;
    selected->selection_score = best_score;
    (void)snprintf(selected->selection_reason, sizeof(selected->selection_reason), "%s",
                   set->items[best].reason);
    selected->confidence = best_score == second_score ? OCCURRENCE_AMBIGUOUS :
                           OCCURRENCE_CONFIRMED;
    if (selected->confidence == OCCURRENCE_AMBIGUOUS) {
        result.status = PROVIDER_OCCURRENCE_AMBIGUOUS;
        (void)snprintf(result.message, sizeof(result.message), "occurrence candidates tied");
        return result;
    }
    result.status = PROVIDER_OK;
    result.message[0] = '\0';
    return result;
}
