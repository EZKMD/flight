#ifndef OPENSKY_TELEMETRY_H
#define OPENSKY_TELEMETRY_H

#include "telemetry_provider.h"

typedef struct { int reserved; } OpenSkyTelemetryContext;

void opensky_telemetry_init(TelemetryProvider *provider,
                            OpenSkyTelemetryContext *context);

#endif
