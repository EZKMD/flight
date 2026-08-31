#include "map_geometry.h"

#include <math.h>

#define MAP_PI 3.14159265358979323846

static double radians(double degrees) { return degrees * MAP_PI / 180.0; }
static double degrees(double radians_value) { return radians_value * 180.0 / MAP_PI; }

static double clamp(double value, double minimum, double maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static double unwrap_longitude(double longitude, double reference)
{
    while (longitude - reference > 180.0) longitude -= 360.0;
    while (longitude - reference < -180.0) longitude += 360.0;
    return longitude;
}

bool geo_coordinate_valid(GeoCoordinate coordinate)
{
    return isfinite(coordinate.latitude) && isfinite(coordinate.longitude) &&
           coordinate.latitude >= -90.0 && coordinate.latitude <= 90.0 &&
           coordinate.longitude >= -180.0 && coordinate.longitude <= 180.0;
}

bool map_great_circle_position(GeoCoordinate origin, GeoCoordinate destination,
                               double progress, GeoCoordinate *position)
{
    double first_latitude;
    double first_longitude;
    double second_latitude;
    double second_longitude;
    double first[3];
    double second[3];
    double dot;
    double angle;
    double a;
    double b;
    double vector[3];
    double length;
    if (position == NULL || !geo_coordinate_valid(origin) ||
        !geo_coordinate_valid(destination)) return false;
    progress = clamp(progress, 0.0, 1.0);
    first_latitude = radians(origin.latitude);
    first_longitude = radians(origin.longitude);
    second_latitude = radians(destination.latitude);
    second_longitude = radians(destination.longitude);
    first[0] = cos(first_latitude) * cos(first_longitude);
    first[1] = cos(first_latitude) * sin(first_longitude);
    first[2] = sin(first_latitude);
    second[0] = cos(second_latitude) * cos(second_longitude);
    second[1] = cos(second_latitude) * sin(second_longitude);
    second[2] = sin(second_latitude);
    dot = clamp(first[0] * second[0] + first[1] * second[1] + first[2] * second[2],
                -1.0, 1.0);
    angle = acos(dot);
    if (angle < 1e-9) {
        *position = origin;
        return true;
    }
    if (fabs(sin(angle)) < 1e-9) {
        double destination_longitude = unwrap_longitude(destination.longitude,
                                                        origin.longitude);
        position->latitude = origin.latitude +
                             (destination.latitude - origin.latitude) * progress;
        position->longitude = origin.longitude +
                              (destination_longitude - origin.longitude) * progress;
        while (position->longitude > 180.0) position->longitude -= 360.0;
        while (position->longitude < -180.0) position->longitude += 360.0;
        return geo_coordinate_valid(*position);
    }
    a = sin((1.0 - progress) * angle) / sin(angle);
    b = sin(progress * angle) / sin(angle);
    vector[0] = a * first[0] + b * second[0];
    vector[1] = a * first[1] + b * second[1];
    vector[2] = a * first[2] + b * second[2];
    length = sqrt(vector[0] * vector[0] + vector[1] * vector[1] +
                  vector[2] * vector[2]);
    if (length <= 0.0) return false;
    position->latitude = degrees(asin(clamp(vector[2] / length, -1.0, 1.0)));
    position->longitude = degrees(atan2(vector[1], vector[0]));
    return geo_coordinate_valid(*position);
}

bool map_route_sample(GeoCoordinate origin, GeoCoordinate destination,
                      size_t sample_count, MapRoute *route)
{
    size_t index;
    if (route == NULL || !geo_coordinate_valid(origin) ||
        !geo_coordinate_valid(destination)) return false;
    if (sample_count < 2U) sample_count = 2U;
    if (sample_count > MAP_ROUTE_MAX_SAMPLES) sample_count = MAP_ROUTE_MAX_SAMPLES;
    route->count = 0U;
    for (index = 0U; index < sample_count; index++) {
        double progress = (double)index / (double)(sample_count - 1U);
        if (!map_great_circle_position(origin, destination, progress,
                                       &route->points[index])) return false;
    }
    route->count = sample_count;
    return true;
}

bool map_viewport_fit(MapViewport *viewport, const MapRoute *route,
                      int pixel_width, int pixel_height, double margin)
{
    double previous_longitude;
    double latitude_sum = 0.0;
    double target_aspect;
    double width;
    double height;
    size_t index;
    if (viewport == NULL || route == NULL || route->count < 2U ||
        pixel_width <= 1 || pixel_height <= 1) return false;
    margin = clamp(margin, 0.0, 0.4);
    previous_longitude = route->points[0].longitude;
    for (index = 0U; index < route->count; index++)
        latitude_sum += route->points[index].latitude;
    viewport->longitude_scale = cos(radians(latitude_sum / (double)route->count));
    if (viewport->longitude_scale < 0.1) viewport->longitude_scale = 0.1;
    viewport->center_longitude = route->points[0].longitude;
    viewport->minimum_x = viewport->maximum_x =
        previous_longitude * viewport->longitude_scale;
    viewport->minimum_y = viewport->maximum_y = route->points[0].latitude;
    for (index = 1U; index < route->count; index++) {
        double longitude = unwrap_longitude(route->points[index].longitude,
                                            previous_longitude);
        double x = longitude * viewport->longitude_scale;
        double y = route->points[index].latitude;
        if (x < viewport->minimum_x) viewport->minimum_x = x;
        if (x > viewport->maximum_x) viewport->maximum_x = x;
        if (y < viewport->minimum_y) viewport->minimum_y = y;
        if (y > viewport->maximum_y) viewport->maximum_y = y;
        previous_longitude = longitude;
    }
    viewport->center_longitude = (viewport->minimum_x + viewport->maximum_x) /
                                 (2.0 * viewport->longitude_scale);
    width = viewport->maximum_x - viewport->minimum_x;
    height = viewport->maximum_y - viewport->minimum_y;
    if (width < 0.01) width = 0.01;
    if (height < 0.01) height = 0.01;
    target_aspect = (double)pixel_width / (double)pixel_height;
    if (width / height < target_aspect) {
        double expansion = height * target_aspect - width;
        viewport->minimum_x -= expansion / 2.0;
        viewport->maximum_x += expansion / 2.0;
    } else {
        double expansion = width / target_aspect - height;
        viewport->minimum_y -= expansion / 2.0;
        viewport->maximum_y += expansion / 2.0;
    }
    viewport->margin = margin;
    return true;
}

static bool project_unwrapped(const MapViewport *viewport, double latitude,
                              double longitude, MapPoint *point)
{
    double x;
    double width;
    double height;
    if (viewport == NULL || point == NULL || !isfinite(latitude) ||
        !isfinite(longitude)) return false;
    x = longitude * viewport->longitude_scale;
    width = viewport->maximum_x - viewport->minimum_x;
    height = viewport->maximum_y - viewport->minimum_y;
    if (width <= 0.0 || height <= 0.0) return false;
    point->x = viewport->margin + (x - viewport->minimum_x) / width *
               (1.0 - 2.0 * viewport->margin);
    point->y = viewport->margin + (viewport->maximum_y - latitude) / height *
               (1.0 - 2.0 * viewport->margin);
    return isfinite(point->x) && isfinite(point->y);
}

bool map_viewport_project(const MapViewport *viewport, GeoCoordinate coordinate,
                          MapPoint *point)
{
    double longitude;
    if (viewport == NULL || !geo_coordinate_valid(coordinate)) return false;
    longitude = unwrap_longitude(coordinate.longitude, viewport->center_longitude);
    return project_unwrapped(viewport, coordinate.latitude, longitude, point);
}

bool map_viewport_project_continuous(const MapViewport *viewport,
                                     GeoCoordinate coordinate,
                                     double *longitude_reference,
                                     MapPoint *point)
{
    double longitude;
    if (viewport == NULL || longitude_reference == NULL ||
        !isfinite(*longitude_reference) || !geo_coordinate_valid(coordinate))
        return false;
    longitude = unwrap_longitude(coordinate.longitude, *longitude_reference);
    *longitude_reference = longitude;
    return project_unwrapped(viewport, coordinate.latitude, longitude, point);
}

static unsigned int clip_code(MapPoint point)
{
    unsigned int code = 0U;
    if (point.x < 0.0) code |= 1U;
    else if (point.x > 1.0) code |= 2U;
    if (point.y < 0.0) code |= 4U;
    else if (point.y > 1.0) code |= 8U;
    return code;
}

bool map_clip_normalized_line(MapPoint *first, MapPoint *second)
{
    unsigned int first_code;
    unsigned int second_code;
    int iteration;
    if (first == NULL || second == NULL || !isfinite(first->x) ||
        !isfinite(first->y) || !isfinite(second->x) || !isfinite(second->y))
        return false;
    first_code = clip_code(*first);
    second_code = clip_code(*second);
    for (iteration = 0; iteration < 16; iteration++) {
        unsigned int outside;
        MapPoint intersection;
        if ((first_code | second_code) == 0U) return true;
        if ((first_code & second_code) != 0U) return false;
        outside = first_code != 0U ? first_code : second_code;
        if ((outside & 8U) != 0U) {
            intersection.x = first->x + (second->x - first->x) *
                             (1.0 - first->y) / (second->y - first->y);
            intersection.y = 1.0;
        } else if ((outside & 4U) != 0U) {
            intersection.x = first->x + (second->x - first->x) *
                             (0.0 - first->y) / (second->y - first->y);
            intersection.y = 0.0;
        } else if ((outside & 2U) != 0U) {
            intersection.y = first->y + (second->y - first->y) *
                             (1.0 - first->x) / (second->x - first->x);
            intersection.x = 1.0;
        } else {
            intersection.y = first->y + (second->y - first->y) *
                             (0.0 - first->x) / (second->x - first->x);
            intersection.x = 0.0;
        }
        intersection.x = clamp(intersection.x, 0.0, 1.0);
        intersection.y = clamp(intersection.y, 0.0, 1.0);
        if (outside == first_code) {
            *first = intersection;
            first_code = clip_code(*first);
        } else {
            *second = intersection;
            second_code = clip_code(*second);
        }
    }
    return false;
}

const char *map_route_direction_arrow(const MapPoint *route, size_t route_count,
                                      double progress)
{
    double route_position;
    double dx;
    double dy;
    double angle;
    size_t center;
    size_t first;
    size_t second;
    if (route == NULL || route_count < 2U) return "→";
    progress = clamp(progress, 0.0, 1.0);
    route_position = progress * (double)(route_count - 1U);
    center = (size_t)(route_position + 0.5);
    if (center >= route_count) center = route_count - 1U;
    first = center > 0U ? center - 1U : center;
    second = center + 1U < route_count ? center + 1U : center;
    dx = route[second].x - route[first].x;
    dy = route[second].y - route[first].y;
    if (fabs(dx) < 1e-9 && fabs(dy) < 1e-9) return "→";
    angle = atan2(-dy, dx) * 180.0 / MAP_PI;
    if (angle >= 157.5 || angle < -157.5) return "←";
    if (angle >= 112.5) return "↖";
    if (angle >= 67.5) return "↑";
    if (angle >= 22.5) return "↗";
    if (angle >= -22.5) return "→";
    if (angle >= -67.5) return "↘";
    if (angle >= -112.5) return "↓";
    return "↙";
}
