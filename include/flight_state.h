#ifndef FLIGHT_STATE_H
#define FLIGHT_STATE_H

#include <stdbool.h>
#include <time.h>

typedef enum {
    FLIGHT_PHASE_SCHEDULED,
    FLIGHT_PHASE_PRE_DEPARTURE,
    FLIGHT_PHASE_TAXIING_DEPARTURE,
    FLIGHT_PHASE_CLIMBING,
    FLIGHT_PHASE_CRUISING,
    FLIGHT_PHASE_DESCENDING,
    FLIGHT_PHASE_AIRBORNE,
    FLIGHT_PHASE_LANDED,
    FLIGHT_PHASE_DELAYED,
    FLIGHT_PHASE_UNAVAILABLE
} FlightPhase;

typedef enum {
    OCCURRENCE_CONFIRMED,
    OCCURRENCE_INFERRED,
    OCCURRENCE_AMBIGUOUS,
    OCCURRENCE_UNAVAILABLE
} OccurrenceConfidence;

typedef enum {
    TELEMETRY_FRESH,
    TELEMETRY_STALE,
    TELEMETRY_UNAVAILABLE,
    TELEMETRY_NOT_EXPECTED
} TelemetryState;

typedef enum {
    PROGRESS_LIVE_GEOSPATIAL,
    PROGRESS_PROVIDER_REPORTED,
    PROGRESS_SCHEDULE_TIME,
    PROGRESS_LANDED,
    PROGRESS_UNAVAILABLE
} ProgressSource;

typedef enum {
    PROGRESS_FALLBACK_NONE,
    PROGRESS_FALLBACK_MISSING_COORDINATES,
    PROGRESS_FALLBACK_ROUTE_TOO_SHORT,
    PROGRESS_FALLBACK_OFF_ROUTE
} ProgressFallbackReason;

typedef enum {
    PHASE_TELEMETRY_DERIVED,
    PHASE_PROVIDER_CONFIRMED,
    PHASE_SCHEDULE_INFERRED,
    PHASE_UNAVAILABLE
} PhaseSource;

typedef enum {
    DISPLAY_LIVE,
    DISPLAY_TRACKING,
    DISPLAY_STALE,
    DISPLAY_SCHEDULED,
    DISPLAY_OFFLINE
} FlightDisplayState;

typedef struct { bool available; double value; } OptionalDouble;
typedef struct { bool available; int value; } OptionalInt;
typedef struct { bool available; time_t value; } OptionalTime;
typedef struct { bool available; bool value; } OptionalBool;

typedef struct {
    char flight_number[16];
    char airline_name[32];
    char airline_code[8];
} FlightIdentity;

typedef struct {
    char model[32];
    char registration[16];
    char icao24[8];
    char callsign[16];
} AircraftState;

typedef struct {
    char name[64];
    char iata[8];
    char icao[8];
    OptionalDouble latitude;
    OptionalDouble longitude;
    char timezone[48];
} AirportState;

typedef struct {
    OptionalTime scheduled_departure;
    OptionalTime estimated_departure;
    OptionalTime actual_departure;
    OptionalTime scheduled_arrival;
    OptionalTime estimated_arrival;
    OptionalTime actual_arrival;
    char departure_display[8];
    char arrival_display[8];
} FlightTiming;

typedef struct {
    OptionalDouble latitude;
    OptionalDouble longitude;
    OptionalInt altitude_feet;
    OptionalInt flight_level;
    OptionalInt groundspeed_knots;
    OptionalInt heading_degrees;
    OptionalInt vertical_rate_fpm;
    OptionalBool on_ground;
    OptionalTime last_position;
    char heading_compass[8];
} FlightPosition;

typedef struct {
    double progress; /* Canonical range: 0.0 to 1.0. */
    bool progress_available;
    ProgressSource progress_source;
    ProgressFallbackReason progress_fallback_reason;
    OptionalInt airborne_minutes;
    OptionalInt remaining_minutes;
} FlightJourney;

typedef struct {
    FlightPhase phase;
    bool delayed;
    bool data_available;
    bool stale;
    OccurrenceConfidence occurrence_confidence;
    TelemetryState telemetry_state;
    PhaseSource phase_source;
    FlightDisplayState display_state;
} FlightStatus;

typedef struct {
    OptionalTime last_updated;
    OptionalTime telemetry_updated;
    OptionalTime telemetry_received;
    bool stale_cache;
    char source[32];
} FlightMetadata;

typedef struct {
    FlightIdentity identity;
    AircraftState aircraft;
    AirportState origin;
    AirportState destination;
    FlightTiming timing;
    FlightPosition position;
    FlightJourney journey;
    FlightStatus status;
    FlightMetadata metadata;
} FlightState;

const char *flight_phase_label(FlightPhase phase);
double flight_progress_clamped(const FlightState *state);
const char *flight_display_label(FlightDisplayState state);

#endif
