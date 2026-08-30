#include "airport_reference.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *iata;
    const char *icao;
    const char *name;
    double latitude;
    double longitude;
    const char *timezone;
} AirportRecord;

static const AirportRecord airports[] = {
#include "../data/airports_generated.inc"
};

typedef struct { const char *iata; const char *timezone; } TimezoneOverride;

static const TimezoneOverride timezone_overrides[] = {
    { "MEL", "Australia/Melbourne" }, { "SYD", "Australia/Sydney" },
    { "PER", "Australia/Perth" }, { "SIN", "Asia/Singapore" },
    { "LHR", "Europe/London" }, { "DXB", "Asia/Dubai" },
    { "LAX", "America/Los_Angeles" }, { "SFO", "America/Los_Angeles" },
    { "JFK", "America/New_York" }, { "DFW", "America/Chicago" },
    { "MIA", "America/New_York" }
};

static const char *timezone_for(const AirportRecord *record)
{
    size_t index;
    if (record->timezone[0] != '\0') return record->timezone;
    for (index = 0; index < sizeof(timezone_overrides) / sizeof(timezone_overrides[0]); index++)
        if (strcmp(record->iata, timezone_overrides[index].iata) == 0)
            return timezone_overrides[index].timezone;
    return "";
}

static const AirportRecord *by_iata(const char *iata)
{
    size_t index;
    for (index = 0; index < sizeof(airports) / sizeof(airports[0]); index++)
        if (strcmp(iata, airports[index].iata) == 0) return &airports[index];
    return NULL;
}

static const AirportRecord *by_icao(const char *icao)
{
    size_t index;
    for (index = 0; index < sizeof(airports) / sizeof(airports[0]); index++)
        if (strcmp(icao, airports[index].icao) == 0) return &airports[index];
    return NULL;
}

static void enrich(AirportState *airport, const AirportRecord *record)
{
    (void)snprintf(airport->name, sizeof(airport->name), "%s", record->name);
    airport->latitude = (OptionalDouble){ true, record->latitude };
    airport->longitude = (OptionalDouble){ true, record->longitude };
    (void)snprintf(airport->timezone, sizeof(airport->timezone), "%s", timezone_for(record));
}

bool airport_reference_pair_valid(const char *iata, const char *icao)
{
    const AirportRecord *record = by_iata(iata);
    return record != NULL && strcmp(record->icao, icao) == 0;
}

AirportValidation airport_reference_normalize(AirportState *airport)
{
    const AirportRecord *iata_record = airport->iata[0] != '\0' ? by_iata(airport->iata) : NULL;
    const AirportRecord *icao_record = airport->icao[0] != '\0' ? by_icao(airport->icao) : NULL;
    if (airport->iata[0] != '\0' && airport->icao[0] != '\0') {
        if (iata_record != NULL && icao_record == iata_record) {
            enrich(airport, iata_record);
            return AIRPORT_VALIDATED;
        }
        if (iata_record == NULL && icao_record == NULL) {
            airport->icao[0] = '\0';
            return AIRPORT_SINGLE_CODE;
        }
        /* IATA is the commercial resolver's primary route identity. */
        airport->icao[0] = '\0';
        if (iata_record != NULL) enrich(airport, iata_record);
        return AIRPORT_MISMATCH;
    }
    if (iata_record != NULL) {
        (void)snprintf(airport->icao, sizeof(airport->icao), "%s", iata_record->icao);
        enrich(airport, iata_record);
        return AIRPORT_ENRICHED;
    }
    if (icao_record != NULL) {
        (void)snprintf(airport->iata, sizeof(airport->iata), "%s", icao_record->iata);
        enrich(airport, icao_record);
        return AIRPORT_ENRICHED;
    }
    if (airport->iata[0] != '\0' || airport->icao[0] != '\0') return AIRPORT_SINGLE_CODE;
    return AIRPORT_UNKNOWN;
}
