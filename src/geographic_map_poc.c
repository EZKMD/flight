#include "geographic_map_poc.h"

#include "coastline_data.h"
#include "map_raster.h"
#include "screen_components.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static GeographicMapPocScene cached_scene;

static int pixel_coordinate(double value, int extent)
{
    int coordinate = (int)(value * (double)(extent - 1) + 0.5);
    if (coordinate < 0) return 0;
    if (coordinate >= extent) return extent - 1;
    return coordinate;
}

static void dotted_line(SubcellCanvas *canvas, MapPoint first, MapPoint second)
{
    int x0 = pixel_coordinate(first.x, canvas->columns * 2);
    int y0 = pixel_coordinate(first.y, canvas->rows * 4);
    int x1 = pixel_coordinate(second.x, canvas->columns * 2);
    int y1 = pixel_coordinate(second.y, canvas->rows * 4);
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    int step = 0;
    for (;;) {
        int twice_error;
        if (step % 3 != 2) (void)subcell_canvas_set(canvas, x0, y0);
        if (x0 == x1 && y0 == y1) break;
        twice_error = 2 * error;
        if (twice_error >= dy) { error += dy; x0 += sx; }
        if (twice_error <= dx) { error += dx; y0 += sy; }
        step++;
    }
}

static bool rasterize_coastline(GeographicMapPocScene *scene)
{
    size_t segment;
    if (!subcell_canvas_init(&scene->coastline, scene->columns, scene->rows))
        return false;
    scene->coastline_segments_drawn = 0U;
    for (segment = 0U; segment < coastline_data_segment_count(); segment++) {
        size_t first_index;
        size_t point_count;
        size_t index;
        bool segment_visible = false;
        if (!coastline_data_segment_bounds(segment, &first_index, &point_count))
            continue;
        for (index = 1U; index < point_count; index++) {
            GeoCoordinate first_geo;
            GeoCoordinate second_geo;
            MapPoint first;
            MapPoint second;
            if (!coastline_data_point(first_index + index - 1U, &first_geo) ||
                !coastline_data_point(first_index + index, &second_geo) ||
                !map_viewport_project(&scene->viewport, first_geo, &first) ||
                !map_viewport_project(&scene->viewport, second_geo, &second))
                continue;
            if (map_clip_normalized_line(&first, &second)) {
                dotted_line(&scene->coastline, first, second);
                segment_visible = true;
            }
        }
        if (segment_visible) scene->coastline_segments_drawn++;
    }
    return true;
}

bool geographic_map_poc_prepare(GeographicMapPocScene *scene,
                                GeoCoordinate origin, GeoCoordinate destination,
                                int columns, int rows, bool include_geography)
{
    MapRoute route;
    size_t index;
    size_t samples;
    if (scene == NULL || columns <= 1 || rows <= 1 ||
        columns > SUBCELL_CANVAS_MAX_COLUMNS ||
        rows > SUBCELL_CANVAS_MAX_ROWS) return false;
    (void)memset(scene, 0, sizeof(*scene));
    scene->origin = origin;
    scene->destination = destination;
    scene->columns = columns;
    scene->rows = rows;
    samples = (size_t)(columns * 2);
    if (!map_route_sample(origin, destination, samples, &route) ||
        !map_viewport_fit(&scene->viewport, &route, columns * 2, rows * 4, 0.12))
        return false;
    scene->route_count = route.count;
    for (index = 0U; index < route.count; index++)
        if (!map_viewport_project(&scene->viewport, route.points[index],
                                  &scene->projected_route[index])) return false;
    if (!subcell_canvas_init(&scene->coastline, columns, rows)) return false;
    if (include_geography && coastline_data_segment_count() > 0U) {
        if (!rasterize_coastline(scene)) return false;
        scene->geography_available = scene->coastline_segments_drawn > 0U;
    }
    scene->valid = true;
    return true;
}

static bool coordinates_from_flight(const FlightState *flight, GeoCoordinate *origin,
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

static bool same_scene(const GeographicMapPocScene *scene, GeoCoordinate origin,
                       GeoCoordinate destination, int columns, int rows)
{
    return scene->valid && scene->columns == columns && scene->rows == rows &&
           scene->origin.latitude == origin.latitude &&
           scene->origin.longitude == origin.longitude &&
           scene->destination.latitude == destination.latitude &&
           scene->destination.longitude == destination.longitude;
}

static bool trustworthy_marker(const FlightState *flight)
{
    return flight->status.telemetry_state == TELEMETRY_FRESH &&
           flight->journey.progress_available &&
           flight->journey.progress_source == PROGRESS_LIVE_GEOSPATIAL;
}

static void render_heading(Frame *frame, const FlightState *flight)
{
    char title[80];
    char route[80];
    (void)snprintf(title, sizeof(title), "%s  GEOGRAPHIC MAP POC",
                   flight->identity.flight_number[0] != '\0' ?
                   flight->identity.flight_number : "--");
    frame_sides(frame, title, flight_display_label(flight->status.display_state));
    (void)snprintf(route, sizeof(route), "%s  →  %s",
                   flight->origin.iata[0] != '\0' ? flight->origin.iata : "---",
                   flight->destination.iata[0] != '\0' ? flight->destination.iata : "---");
    frame_center(frame, route, 0);
}

static void render_status(Frame *frame, const FlightState *flight, bool live_marker)
{
    char status[80];
    if (flight->status.phase == FLIGHT_PHASE_LANDED)
        frame_center(frame, "100.0% · LANDED", 0);
    else if (live_marker) {
        (void)snprintf(status, sizeof(status), "%.1f%% · LIVE POSITION",
                       flight_progress_clamped(flight) * 100.0);
        frame_center(frame, status, 0);
    } else if (flight->journey.progress_available &&
               flight->journey.progress_source == PROGRESS_SCHEDULE_TIME) {
        (void)snprintf(status, sizeof(status), "SCHEDULE PROGRESS %.1f%% · NO LIVE POSITION",
                       flight_progress_clamped(flight) * 100.0);
        frame_center(frame, status, 0);
    } else frame_center(frame, "NO LIVE POSITION", 0);
}

void geographic_map_poc_render(Frame *frame, const FlightState *flight,
                               const AnimationState *animation,
                               const Layout *layout)
{
    GeoCoordinate origin;
    GeoCoordinate destination;
    GeoCoordinate marker_geo;
    MapPoint marker_point;
    MapPoint *marker = NULL;
    MapRaster raster;
    bool live_marker;
    bool landed;
    int columns;
    int rows;
    int row;
    int column;
    render_heading(frame, flight);
    frame_blank(frame);
    if (layout->mode == LAYOUT_COMPACT || layout->mode == LAYOUT_TINY ||
        layout->height < 14) {
        frame_center(frame, "GEOGRAPHIC MAP REQUIRES MORE SPACE", 0);
        frame_blank(frame);
        data_freshness_render(frame, flight, animation, time(NULL));
        return;
    }
    if (!coordinates_from_flight(flight, &origin, &destination)) {
        frame_center(frame, "ROUTE GEOMETRY UNAVAILABLE", 0);
        return;
    }
    columns = frame->width;
    if (columns > SUBCELL_CANVAS_MAX_COLUMNS) columns = SUBCELL_CANVAS_MAX_COLUMNS;
    rows = layout->height - 8;
    if (layout->mode == LAYOUT_WIDE && rows > 12) rows = 12;
    else if (rows > 10) rows = 10;
    if (!same_scene(&cached_scene, origin, destination, columns, rows) &&
        !geographic_map_poc_prepare(&cached_scene, origin, destination,
                                    columns, rows, true)) {
        frame_center(frame, "GEOGRAPHIC MAP UNAVAILABLE", 0);
        return;
    }
    live_marker = trustworthy_marker(flight);
    landed = flight->status.phase == FLIGHT_PHASE_LANDED;
    if (landed) marker_geo = destination;
    else if (live_marker && !map_great_circle_position(origin, destination,
                             flight_progress_clamped(flight), &marker_geo))
        live_marker = false;
    if ((landed || live_marker) &&
        map_viewport_project(&cached_scene.viewport, marker_geo, &marker_point))
        marker = &marker_point;
    if (!map_raster_render(&raster, MAP_RASTER_BRAILLE,
                           cached_scene.projected_route, cached_scene.route_count,
                           marker, columns, rows)) return;
    for (row = 0; row < rows; row++)
        for (column = 0; column < columns; column++)
            if (raster.cells[row][column] == (uint32_t)' ')
                raster.cells[row][column] =
                    subcell_canvas_codepoint(&cached_scene.coastline, column, row);
    for (row = 0; row < rows; row++) {
        char line[FRAME_LINE_CAPACITY];
        if (map_raster_row_utf8(&raster, row, line, sizeof(line))) frame_add(frame, line);
    }
    if (live_marker && raster.marker_available && animation != NULL &&
        animation->heartbeat != NULL && strcmp(animation->heartbeat, "•") == 0)
        frame_at(frame, raster.marker_column, "✈", "");
    else frame_blank(frame);
    render_status(frame, flight, live_marker);
    frame_center(frame, cached_scene.geography_available ?
                 "EXPERIMENTAL · NATURAL EARTH 110M" :
                 "EXPERIMENTAL · GEOGRAPHY UNAVAILABLE", 0);
    data_freshness_render(frame, flight, animation, time(NULL));
}
