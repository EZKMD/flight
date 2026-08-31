# Geographic route map proof of concept

## Status and purpose

This is an experimental, unapproved visual on the disposable
`poc/geographic-map` branch. It asks whether real coastline context improves route
comprehension enough to justify a product feature. It does not replace the validated
RouteMapVisual: `--view route` is unchanged, while the experiment is entered explicitly
with `--view geo` and is deliberately absent from the `v` key cycle.

The second POC pass adds experimental styling and a rendering-layer toggle. Press `g`
inside `--view geo` to switch between `GEO ON` and `GEO OFF`. This does not change the
visual mode, viewport, route geometry, provider state, telemetry history, or refresh
schedule.

## Geography data

The generated runtime artifact uses the Natural Earth **1:110m coastline**, pinned to
the `v5.1.2` release:

- source: <https://github.com/nvkelso/natural-earth-vector/blob/v5.1.2/geojson/ne_110m_coastline.geojson>
- terms: <https://www.naturalearthdata.com/about/terms-of-use/>
- license: public domain

The source GeoJSON contains 134 line features and 5,128 source vertices. The generator
removes metadata, deduplicates adjacent coordinates, quantizes coordinates to 0.01°,
and emits 5,127 points plus segment offsets. The checked-in C include is approximately
109 KB of source text; its compiled read-only coastline payload is approximately 21 KB.
There is no runtime parser, network request, GIS library, or additional provider call.

To regenerate from an explicitly downloaded source file:

```sh
curl -fsSL \
  https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.2/geojson/ne_110m_coastline.geojson \
  -o /tmp/ne_110m_coastline.geojson
python3 tools/generate_coastline.py \
  /tmp/ne_110m_coastline.geojson data/coastline_110m_generated.inc
```

Python is a development-time regeneration tool only; it is not required to build or run
`flight` from the checked-in source.

## Projection and framing

Coastlines and the great-circle route use the same existing route-local equirectangular
projection. Longitude is unwrapped around the route, horizontal distance is scaled by
the cosine of the route's mean latitude, and one `MapViewport` transforms both layers.
The viewport aspect-fits the sampled great-circle and reserves a 12% geographic margin.
Coastline segments are clipped at that viewport.

The renderer caches the projected/rasterized coastline by origin, destination, terminal
map width, and terminal map height. Heartbeat frames composite the cached geography with
the route and current marker without reprojecting all 5,127 coastline points.

Coastlines are unfilled, broken Braille strokes. The continuous route is rendered on a
separate layer and wins every cell collision; `●`, `◆`, and the pulsing `✈` then win over
both. Coastline cells use standard ANSI dim, route/endpoints remain default-bright, and
`◆`/`✈` use standard bright cyan. Glyph structure—not hue—continues to distinguish every
layer. Styling is stored separately from glyph strings so ANSI bytes do not affect width,
clipping, or alignment. Each styled run is reset before another layer or line is written.

Set `NO_COLOR` to any value to omit all experimental SGR styling:

```sh
NO_COLOR=1 ./flight QF9 --fixture cruising --view geo
```

Wide and medium layouts support direct `g` comparisons with one stationary cached scene.
With geography enabled, compact, tiny, or height-constrained terminals still show
`GEOGRAPHIC MAP REQUIRES MORE SPACE`. Toggle geography off with `g` to use the existing
responsive route-only renderer at those sizes.

## Visual inspection

Build and run the deterministic offline MEL–LHR case:

```sh
make
./flight QF9 --fixture cruising --view geo
```

While it is running, press `g` twice and verify that the route, endpoints, and marker stay
fixed while only the coastline disappears and returns.

Provider-backed visual candidates (availability depends on the provider resolving a
current occurrence; none is required by the automated tests):

```sh
./flight QF9 --view geo       # MEL–LHR style long haul
./flight QR905 --view geo     # MEL–DOH
./flight BA281 --view geo     # LHR–LAX
./flight QF609 --view geo     # BNE–MEL regional
./flight BA281 --view geo     # live candidate when currently active
```

The existing production comparison remains:

```sh
./flight BA281 --view route
```

No-position and monochrome comparisons:

```sh
./flight QF9 --fixture scheduled --view geo
./flight QF9 --fixture unavailable --view geo
NO_COLOR=1 ./flight BA281 --view geo
```

Resize from wide to medium to compact and back. In compact mode, press `g` to expose the
route-only fallback. Confirm `v` still cycles only aircraft → altitude → route.

Deterministic tests cover MEL–LHR, MEL–DOH, LHR–LAX, SYD–SIN, an antimeridian crossing,
and a high-latitude route without consuming API quota.

## Current limitations

- The route-local projection is coherent but increasingly distorted at high latitudes.
- Framing is route-driven, so oceanic routes may contain little recognizable coastline.
- At terminal resolution, even dim 1:110m coastline linework can remain busy on long-haul
  views, although hierarchy is substantially clearer than in the monochrome first pass.
- ANSI dim and bright cyan are conservative SGR assumptions, but exact intensity varies by
  terminal theme; `NO_COLOR` preserves the structural monochrome treatment.
- There are no country borders, filled land, labels, pan, zoom, trail, weather, traffic,
  waypoints, or additional map providers.
- The experiment is intentionally not advertised in the normal README workflow.

## Keep or discard

Keep and refine only if manual comparison shows that geography materially improves route
comprehension while the route remains dominant and the interface still feels minimal.
Defer or discard if the coastline is noisy, misleading, visually stronger than the route,
or not valuable enough to justify the data and rendering complexity. A clean build alone
is not evidence that this belongs in the released product.

Current implementation assessment: styling makes the route and authoritative marker much
easier to separate from geography, and the stationary `g` comparison is useful. Short/local
routes remain the strongest case. Long-haul maps are improved but can still be dense. The
recommended status remains experimental pending direct product-design review.
