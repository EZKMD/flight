#ifndef FLIGHT_CACHE_H
#define FLIGHT_CACHE_H

#include <stdbool.h>
#include <time.h>
#include "flight_resolver.h"

bool cache_load_resolved(const char *flight_number, const char *date, time_t now,
                         long ttl_seconds, ResolvedFlight *resolved);
bool cache_store_resolved(const char *flight_number, const char *date,
                          const ResolvedFlight *resolved, time_t now);

#endif
