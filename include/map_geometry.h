#ifndef MAP_GEOMETRY_H
#define MAP_GEOMETRY_H

#include <stdbool.h>
#include <stddef.h>

#define MAP_ROUTE_MAX_SAMPLES 256

typedef struct {
    double latitude;
    double longitude;
} GeoCoordinate;

typedef struct {
    double x;
    double y;
} MapPoint;

typedef struct {
    GeoCoordinate points[MAP_ROUTE_MAX_SAMPLES];
    size_t count;
} MapRoute;

typedef struct {
    double center_longitude;
    double longitude_scale;
    double minimum_x;
    double maximum_x;
    double minimum_y;
    double maximum_y;
    double margin;
} MapViewport;

bool geo_coordinate_valid(GeoCoordinate coordinate);
bool map_great_circle_position(GeoCoordinate origin, GeoCoordinate destination,
                               double progress, GeoCoordinate *position);
bool map_route_sample(GeoCoordinate origin, GeoCoordinate destination,
                      size_t sample_count, MapRoute *route);
bool map_viewport_fit(MapViewport *viewport, const MapRoute *route,
                      int pixel_width, int pixel_height, double margin);
bool map_viewport_project(const MapViewport *viewport, GeoCoordinate coordinate,
                          MapPoint *point);
bool map_clip_normalized_line(MapPoint *first, MapPoint *second);

#endif
