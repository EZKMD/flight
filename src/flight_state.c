#include "flight_state.h"

const char *flight_phase_label(FlightPhase phase)
{
    switch (phase) {
        case FLIGHT_PHASE_SCHEDULED: return "SCHEDULED";
        case FLIGHT_PHASE_PRE_DEPARTURE: return "BOARDING";
        case FLIGHT_PHASE_TAXIING_DEPARTURE: return "TAXIING";
        case FLIGHT_PHASE_CLIMBING: return "CLIMBING";
        case FLIGHT_PHASE_CRUISING: return "CRUISING";
        case FLIGHT_PHASE_DESCENDING: return "DESCENDING";
        case FLIGHT_PHASE_AIRBORNE: return "AIRBORNE";
        case FLIGHT_PHASE_LANDED: return "LANDED";
        case FLIGHT_PHASE_DELAYED: return "DELAYED";
        case FLIGHT_PHASE_UNAVAILABLE: return "UNAVAILABLE";
    }
    return "UNKNOWN";
}

double flight_progress_clamped(const FlightState *state)
{
    if (state->journey.progress < 0.0) return 0.0;
    if (state->journey.progress > 1.0) return 1.0;
    return state->journey.progress;
}

const char *flight_display_label(FlightDisplayState state)
{
    switch (state) {
        case DISPLAY_LIVE: return "LIVE";
        case DISPLAY_TRACKING: return "TRACKING";
        case DISPLAY_STALE: return "STALE";
        case DISPLAY_SCHEDULED: return "SCHEDULED";
        case DISPLAY_OFFLINE: return "OFFLINE";
    }
    return "OFFLINE";
}
