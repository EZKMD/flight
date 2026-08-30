#include "geospatial_progress.h"

#include <math.h>

#define EARTH_RADIUS_KM 6371.0088
#define MIN_ROUTE_KM 10.0
#define MIN_CROSS_TRACK_LIMIT_KM 200.0
#define CROSS_TRACK_ROUTE_FRACTION 0.25

static double radians(double degrees)
{
    return degrees * 3.14159265358979323846 / 180.0;
}

static double angular_distance(double lat1, double lon1, double lat2, double lon2)
{
    double dlat = radians(lat2 - lat1);
    double dlon = radians(lon2 - lon1);
    double a = sin(dlat / 2.0) * sin(dlat / 2.0) + cos(radians(lat1)) *
               cos(radians(lat2)) * sin(dlon / 2.0) * sin(dlon / 2.0);
    if (a > 1.0) a = 1.0;
    return 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}

static double bearing(double lat1, double lon1, double lat2, double lon2)
{
    double first = radians(lat1);
    double second = radians(lat2);
    double dlon = radians(lon2 - lon1);
    return atan2(sin(dlon) * cos(second),
                 cos(first) * sin(second) - sin(first) * cos(second) * cos(dlon));
}

GeospatialProgressResult geospatial_progress(double origin_latitude, double origin_longitude,
                                              double destination_latitude,
                                              double destination_longitude,
                                              double position_latitude, double position_longitude,
                                              double *progress, double *cross_track_km)
{
    double route = angular_distance(origin_latitude, origin_longitude,
                                    destination_latitude, destination_longitude);
    double to_position;
    double route_bearing;
    double position_bearing;
    double cross_track;
    double along_track;
    double route_km = route * EARTH_RADIUS_KM;
    double limit;
    if (route_km < MIN_ROUTE_KM) return GEO_PROGRESS_ROUTE_TOO_SHORT;
    to_position = angular_distance(origin_latitude, origin_longitude,
                                   position_latitude, position_longitude);
    route_bearing = bearing(origin_latitude, origin_longitude,
                            destination_latitude, destination_longitude);
    position_bearing = bearing(origin_latitude, origin_longitude,
                               position_latitude, position_longitude);
    cross_track = asin(sin(to_position) * sin(position_bearing - route_bearing));
    along_track = atan2(sin(to_position) * cos(position_bearing - route_bearing),
                        cos(to_position));
    *cross_track_km = fabs(cross_track * EARTH_RADIUS_KM);
    limit = route_km * CROSS_TRACK_ROUTE_FRACTION;
    if (limit < MIN_CROSS_TRACK_LIMIT_KM) limit = MIN_CROSS_TRACK_LIMIT_KM;
    if (*cross_track_km > limit) return GEO_PROGRESS_OFF_ROUTE;
    *progress = along_track / route;
    if (*progress < 0.0) *progress = 0.0;
    if (*progress > 1.0) *progress = 1.0;
    return GEO_PROGRESS_VALID;
}
