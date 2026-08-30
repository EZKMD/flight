#ifndef AIRLABS_RESOLVER_H
#define AIRLABS_RESOLVER_H

#include "flight_resolver.h"

typedef struct { const char *api_key; } AirLabsResolverContext;

void airlabs_resolver_init(FlightResolver *resolver, AirLabsResolverContext *context,
                           const char *api_key);

#endif
