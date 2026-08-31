#include "route_map.h"

#include "map_geometry.h"
#include "screen_components.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

MapRasterBackend route_map_backend_for_layout(LayoutMode mode)
{
    return mode == LAYOUT_WIDE || mode == LAYOUT_MEDIUM ?
           MAP_RASTER_BRAILLE : MAP_RASTER_COMPAT;
}

static bool route_coordinates(const FlightState *flight, GeoCoordinate *origin,
                              GeoCoordinate *destination)
{
    if (!flight->origin.latitude.available || !flight->origin.longitude.available ||
        !flight->destination.latitude.available ||
        !flight->destination.longitude.available) return false;
    origin->latitude = flight->origin.latitude.value;
    origin->longitude = flight->origin.longitude.value;
    destination->latitude = flight->destination.latitude.value;
    destination->longitude = flight->destination.longitude.value;
    return geo_coordinate_valid(*origin) && geo_coordinate_valid(*destination);
}

static bool live_marker_available(const FlightState *flight)
{
    return flight->status.telemetry_state == TELEMETRY_FRESH &&
           flight->journey.progress_available &&
           flight->journey.progress_source == PROGRESS_LIVE_GEOSPATIAL;
}

static void render_context(Frame *frame, const FlightState *flight)
{
    char title[64];
    char origin[32];
    char destination[32];
    (void)snprintf(title, sizeof(title), "%s  ROUTE MAP",
                   flight->identity.flight_number[0] != '\0' ?
                   flight->identity.flight_number : "--");
    frame_sides(frame, title, flight_display_label(flight->status.display_state));
    if (flight->origin.icao[0] != '\0')
        (void)snprintf(origin, sizeof(origin), "%s / %s",
                       flight->origin.iata, flight->origin.icao);
    else (void)snprintf(origin, sizeof(origin), "%s", flight->origin.iata);
    if (flight->destination.icao[0] != '\0')
        (void)snprintf(destination, sizeof(destination), "%s / %s",
                       flight->destination.iata, flight->destination.icao);
    else (void)snprintf(destination, sizeof(destination), "%s",
                       flight->destination.iata);
    frame_sides(frame, origin[0] != '\0' ? origin : "---",
                destination[0] != '\0' ? destination : "---");
}

static void render_status(Frame *frame, const FlightState *flight, bool live_marker)
{
    char status[96];
    if (flight->status.phase == FLIGHT_PHASE_LANDED) {
        frame_center(frame, "100.0% · LANDED", 0);
        return;
    }
    if (live_marker) {
        char heading[32] = "";
        if (flight->position.heading_degrees.available)
            (void)snprintf(heading, sizeof(heading), " · %03d° %s",
                           flight->position.heading_degrees.value,
                           flight->position.heading_compass);
        (void)snprintf(status, sizeof(status), "%.1f%%%s",
                       flight_progress_clamped(flight) * 100.0, heading);
        frame_center(frame, status, 0);
    } else if (flight->journey.progress_available &&
               flight->journey.progress_source == PROGRESS_SCHEDULE_TIME) {
        (void)snprintf(status, sizeof(status), "SCHEDULE PROGRESS %.1f%%",
                       flight_progress_clamped(flight) * 100.0);
        frame_center(frame, status, 0);
    } else frame_center(frame, "NO LIVE POSITION", 0);
}

static void render_aircraft_annotation(Frame *frame, const MapRaster *raster,
                                       const AnimationState *animation, bool show)
{
    char line[FRAME_LINE_CAPACITY];
    int column;
    bool pulse = animation != NULL && animation->heartbeat != NULL &&
                 strcmp(animation->heartbeat, "•") == 0;
    if (!show || !raster->marker_available || !pulse) {
        frame_blank(frame);
        return;
    }
    column = raster->marker_column;
    if (column >= frame->width) column = frame->width - 1;
    (void)snprintf(line, sizeof(line), "%*s✈", column, "");
    frame_add(frame, line);
}

void route_map_visual_render(Frame *frame, const FlightState *flight,
                             const AnimationState *animation, const Layout *layout)
{
    GeoCoordinate origin;
    GeoCoordinate destination;
    GeoCoordinate marker_geo;
    MapRoute route;
    MapViewport viewport;
    MapPoint projected[MAP_ROUTE_MAX_SAMPLES];
    MapPoint marker_point;
    MapPoint *marker = NULL;
    MapRaster raster;
    MapRasterBackend backend = route_map_backend_for_layout(layout->mode);
    bool live_marker = live_marker_available(flight);
    bool landed_marker = flight->status.phase == FLIGHT_PHASE_LANDED;
    bool marker_geo_available = false;
    int columns = frame->width;
    int rows;
    int pixel_width;
    int pixel_height;
    size_t samples;
    size_t index;
    render_context(frame, flight);
    frame_blank(frame);
    if (!route_coordinates(flight, &origin, &destination)) {
        frame_center(frame, "ROUTE GEOMETRY UNAVAILABLE", 0);
        frame_blank(frame);
        data_freshness_render(frame, flight, animation, time(NULL));
        return;
    }
    rows = layout->height - 8;
    if (layout->mode == LAYOUT_WIDE && rows > 12) rows = 12;
    else if (layout->mode == LAYOUT_MEDIUM && rows > 10) rows = 10;
    else if (layout->mode == LAYOUT_COMPACT && rows > 6) rows = 6;
    else if (layout->mode == LAYOUT_TINY) rows = 2;
    if (rows < 2) rows = 2;
    if (columns > MAP_RASTER_MAX_COLUMNS) columns = MAP_RASTER_MAX_COLUMNS;
    pixel_width = backend == MAP_RASTER_BRAILLE ? columns * 2 : columns;
    pixel_height = backend == MAP_RASTER_BRAILLE ? rows * 4 : rows;
    samples = (size_t)(pixel_width * 2);
    if (!map_route_sample(origin, destination, samples, &route) ||
        !map_viewport_fit(&viewport, &route, pixel_width, pixel_height, 0.04)) {
        frame_center(frame, "ROUTE GEOMETRY UNAVAILABLE", 0);
        return;
    }
    for (index = 0U; index < route.count; index++)
        if (!map_viewport_project(&viewport, route.points[index], &projected[index])) return;
    if (layout->mode == LAYOUT_TINY) {
        for (index = 0U; index < route.count; index++) {
            projected[index].x = (double)index / (double)(route.count - 1U);
            projected[index].y = 0.5;
        }
    }
    if (landed_marker) {
        marker_geo = destination;
        marker_geo_available = true;
    } else if (live_marker)
        marker_geo_available = map_great_circle_position(origin, destination,
                               flight_progress_clamped(flight), &marker_geo);
    if (marker_geo_available &&
        map_viewport_project(&viewport, marker_geo, &marker_point)) marker = &marker_point;
    if (marker != NULL && layout->mode == LAYOUT_TINY) {
        marker_point.x = flight_progress_clamped(flight);
        marker_point.y = 0.5;
    }
    if (!map_raster_render(&raster, backend, projected, route.count, marker,
                           columns, rows)) {
        frame_center(frame, "ROUTE RENDERING UNAVAILABLE", 0);
        return;
    }
    for (index = 0U; index < (size_t)raster.rows; index++) {
        char line[FRAME_LINE_CAPACITY];
        if (map_raster_row_utf8(&raster, (int)index, line, sizeof(line))) frame_add(frame, line);
    }
    render_aircraft_annotation(frame, &raster, animation, live_marker);
    render_status(frame, flight, live_marker);
    frame_blank(frame);
    data_freshness_render(frame, flight, animation, time(NULL));
}
