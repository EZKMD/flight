#include "telemetry_provider.h"

#include <string.h>

bool telemetry_snapshot_matches(const TelemetryRequest *request,
                                const TelemetrySnapshot *snapshot)
{
    if (request->icao24[0] == '\0' || snapshot->icao24[0] == '\0') return false;
    return strcmp(request->icao24, snapshot->icao24) == 0;
}
