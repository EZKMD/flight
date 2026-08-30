#ifndef PROVIDER_DEBUG_H
#define PROVIDER_DEBUG_H

#include <stdio.h>
#include "provider.h"

void provider_debug_print(FILE *output, const LiveDataProviderContext *context,
                          const FlightState *state, ProviderResult final_result);

#endif
