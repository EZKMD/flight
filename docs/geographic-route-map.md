# Geographic route map

## Product model

`RouteMapVisual` is one visual with an optional geography layer. The route is the
subject and the map is context. Start it directly with:

```sh
./flight BA281 --view route
```

The normal `v` cycle is `aircraft → altitude → route → aircraft`. Route mode starts
with geography off. While it is active, `g` toggles geography without changing visual
mode, route geometry, position, provider state, telemetry history, or refresh timing.
The preference lasts for the current process and is not written to disk.

Wide and medium layouts support geography. Compact, tiny, and height-constrained
layouts keep rendering the responsive route fallback even when the preference is on.
Geography returns automatically after the terminal becomes large enough again.

## Scene and viewport

The route renderer composes four layers:

```text
RouteMapVisual
├── GeographyLayer       optional
├── RouteLayer           always
├── EndpointLayer        always
└── PositionLayer        conditional
```

Coastlines and the sampled great-circle use one route-local equirectangular projection
and one `MapViewport`. Longitude is unwrapped around the route, horizontal distance is
scaled by the cosine of its mean latitude, and coastline segments are clipped to the
viewport. Longitude continuity is also preserved within every coastline polyline to
avoid false antimeridian seams.

Wide and medium route maps use a single 12% framing margin in both geography states.
Consequently `g` changes only coastline cells: route samples, endpoints, `◆`, and the
direction-arrow anchor remain stationary. Compact and tiny layouts use the established
compatibility renderer.

The projected and rasterized coastline is cached by origin, destination, map width, and
map height. Those values capture the current fixed projection policy. Heartbeat frames,
marker pulses, and `g` toggles reuse the cached scene.

## Geography data

The generated runtime artifact uses Natural Earth **1:110m coastline**, pinned to
`v5.1.2`:

- source: <https://github.com/nvkelso/natural-earth-vector/blob/v5.1.2/geojson/ne_110m_coastline.geojson>
- terms: <https://www.naturalearthdata.com/about/terms-of-use/>
- license: public domain

The source has 134 line features and 5,128 vertices. Generation removes metadata,
deduplicates adjacent coordinates, quantizes coordinates to 0.01°, and emits 5,127
points plus segment offsets. The checked-in artifact requires no runtime parser, map
request, GIS library, Python installation, or additional provider credential.

To regenerate it from an explicitly downloaded source file:

```sh
curl -fsSL \
  https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.2/geojson/ne_110m_coastline.geojson \
  -o /tmp/ne_110m_coastline.geojson
python3 tools/generate_coastline.py \
  /tmp/ne_110m_coastline.geojson data/coastline_110m_generated.inc
```

Python is a development-time regeneration tool only.

## Visual hierarchy and semantics

- Coastline: dim, broken Braille strokes.
- Route: continuous and default-bright; it wins collisions with geography.
- Endpoints: bright `●` markers.
- Trusted position: bright-cyan `◆`, which remains authoritative.
- Activity annotation: pulsing bright-cyan `← ↖ ↑ ↗ → ↘ ↓ ↙`, selected from the
  local tangent of the projected great-circle.

The position layer appears only for validated live geospatial progress or a confirmed
destination on landing. Schedule-time progress is never drawn as a precise geographic
position.

Styles are stored separately from glyphs, so ANSI bytes cannot affect cell width,
clipping, or alignment. Every styled run is reset. Set `NO_COLOR` to omit ANSI styling
while retaining the structural distinction between broken coastlines, continuous route,
and explicit markers:

```sh
NO_COLOR=1 ./flight BA281 --view route
```

## Validation

Deterministic coverage includes MEL–LHR, MEL–DOH, LHR–LAX, SYD–SIN, a short regional
route, an antimeridian crossing, a high-latitude route, scheduled and landed states,
missing live position, responsive fallbacks, style resets, `NO_COLOR`, stationary layer
toggling, and all eight direction arrows. The tests make no network requests.

## Limitations and extension points

The route-local projection becomes less representative at high latitudes. Oceanic routes
may show little recognizable coastline, and terminal themes vary in how strongly they
render ANSI dim and bright cyan. This is route context rather than a navigation-grade map.

Country borders, filled land, labels, trail history, waypoints, airports, nearby traffic,
weather, airspace, pan/zoom, alternative projections, and playback are not implemented.
They remain possible scene layers or backend improvements, not product commitments.

## History

Geography originated on `poc/geographic-map` as an isolated product-design experiment.
After manual regional, long-haul, live-position, resize, colour, seam, and control testing,
the experiment was accepted and consolidated into the production route visual.
