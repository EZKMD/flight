#ifndef GEOSPATIAL_PROGRESS_H
#define GEOSPATIAL_PROGRESS_H

#include <stdbool.h>

typedef enum {
    GEO_PROGRESS_VALID,
    GEO_PROGRESS_ROUTE_TOO_SHORT,
    GEO_PROGRESS_OFF_ROUTE
} GeospatialProgressResult;

GeospatialProgressResult geospatial_progress(double origin_latitude, double origin_longitude,
                                              double destination_latitude,
                                              double destination_longitude,
                                              double position_latitude, double position_longitude,
                                              double *progress, double *cross_track_km);

#endif
