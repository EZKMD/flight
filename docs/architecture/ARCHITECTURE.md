# Architecture notes

This guide records the implementation observed in `include/` and `src/`. It does not prescribe runtime changes.

## System and data flow

Live mode composes a `FlightResolver` backed by AirLabs and a `TelemetryProvider` backed by OpenSky inside `FlightDataProvider`. Initial load checks a six-hour resolver cache. On a miss it resolves a commercial occurrence, selects a leg, stores the `ResolvedFlight`, normalizes it into `FlightState`, then requests OpenSky telemetry using the selected aircraft's ICAO24 (with callsign retained in the request). A resolver failure may fall back to cached occurrence metadata up to 30 days old. Periodic live refreshes reuse the selected occurrence and fetch telemetry only.

Provider-specific JSON remains inside `airlabs_resolver.c` and `opensky_telemetry.c`. Both use `http_transport.c`, `json.c`, and `ProviderResult`; neither payload shape reaches the renderer. `normalizer.c` converts the provider-neutral `ResolvedFlight` and `TelemetrySnapshot` into `FlightState`, including SI-to-display unit conversion, freshness, phase, duration, and progress derivation.

Mock mode bypasses resolver and telemetry interfaces: `MockDataProvider` writes deterministic `FlightState` fixtures and refreshes them locally.

## Runtime loops

The main loop sleeps for 50 ms between iterations and independently checks:

- terminal resize signals, which recalculate `Layout` and request redraw;
- non-blocking keyboard input (`q` quit, `r` refresh, `v` switch visual, `f` next fixture in mock mode);
- animation every 100 ms, which mutates only `AnimationState`;
- periodic rendering every 1 second;
- provider refresh at 300 seconds for live mode or 15 seconds for mock mode.

Manual `r` and the provider deadline are the only ordinary refresh triggers. Animation, layout selection, resize handling, and rendering do not call provider functions or initiate HTTP requests. Failures defer data refresh by status: rate-limit retry advice, 60 seconds for timeout/unavailable, and one hour for authentication or missing API key.

After each provider load or refresh, the runtime may append the accepted fresh
normalized observation to `TelemetryHistory`. Duplicate, out-of-order, stale,
and missing observations do not create repeated points. Collection never calls
a provider and uses the provider's existing refresh interval to define graph
gap semantics.

## FlightState

`FlightState` has nine top-level logical areas:

- `FlightIdentity`: flight number, airline name, airline code.
- `AircraftState`: model, registration, ICAO24, callsign.
- two `AirportState` values: origin and destination name/codes, optional coordinates, timezone.
- `FlightTiming`: scheduled, estimated, and actual departure/arrival plus display strings.
- `FlightPosition`: optional coordinates, altitude/flight level, speed, heading, vertical rate, ground flag, and last-position time.
- `FlightJourney`: canonical 0–1 progress, source, airborne minutes, and remaining minutes.
- `FlightStatus`: phase, delayed/data/stale flags, occurrence confidence, telemetry state, phase source, and display state.
- `FlightMetadata`: provider/source timestamps, stale-cache flag, and source label.

Optional measurements and timestamps explicitly pair an `available` flag with a value. Progress prefers fresh geospatial telemetry, falls back to schedule time, clamps to 0–1, and records its `ProgressSource`.

## Phase derivation

Provider normalization yields `LANDED` from landed status or actual arrival, `DELAYED` from a positive pre-departure delay, `AIRBORNE` from active/en-route status, `PRE_DEPARTURE` within the two-hour window, `SCHEDULED` for a future departure, and otherwise `UNAVAILABLE`.

Fresh telemetry refines the phase. On-ground aircraft become `LANDED`, `TAXIING_DEPARTURE`, or `PRE_DEPARTURE`. Climb/descent enter beyond ±500 fpm and retain their prior phase to ±300 fpm; taxi enters above 5 knots and exits below 3 knots. Known flight level and a neutral vertical rate yield `CRUISING`; otherwise the phase is `AIRBORNE`. Stale telemetry does not refine the provider-derived phase.

## Renderer architecture

```text
FlightState + AnimationState + Layout
                    |
                    v
               renderer_draw
                    |
          +---------+----------+
          |                    |
  screen_components      VisualViewport
                               |
          AircraftVisual / AltitudeProfileVisual / RouteMapVisual
                                                     |
                          MapViewport + Projection + Scene Layers
                                                     |
                                      Braille / Compat Raster Backend
                               |
                              Frame
                               |
                       ANSI terminal output
```

`layout_select` uses the current terminal size:

| Mode | Width |
| --- | ---: |
| Wide | 120+ |
| Medium | 80–119 |
| Compact | 50–79 |
| Tiny | below 50 |

The minimum supported size is 32×9. Height also controls artwork (16+), normal versus small artwork (21+), times (19+), and telemetry labels (23+). Tiny or very short terminals use the compact summary; smaller than 32×9 renders a minimum-size notice.

## Provider abstraction

`FlightResolver` is a context pointer, `resolve` function, and name. It maps `FlightResolveRequest` to `ResolvedFlight` plus `ProviderResult`. `TelemetryProvider` has the same shape around `TelemetryRequest` to `TelemetrySnapshot`. `FlightDataProvider` supplies runtime-level `load`, `refresh`, refresh interval, context, and name. This layering isolates provider API structures from `FlightState` and all rendering modules.

## Repository/module map

```text
main.c
├── provider.c
│   ├── airlabs_resolver.c ── flight_candidate.c / airport_reference.c
│   ├── opensky_telemetry.c
│   ├── http_transport.c / json.c / cache.c
│   ├── provider_result.c / provider_debug.c
│   ├── normalizer.c ── flight_state.c
│   └── mock_provider.c
├── runtime.c / animation.c / input.c / terminal.c
└── renderer.c
    ├── layout.c
    ├── screen_components.c
    ├── visual_viewport.c
    │   ├── aircraft_visual.c ── artwork.c
    │   ├── altitude_profile.c ← telemetry_history.c
    │   └── route_map.c
    │       ├── map_geometry.c
    │       ├── map_raster.c
    │       └── subcell_canvas.c
    └── frame.c ── terminal output
```

## Explicit future extension points

`AppMode` separates the implemented single-flight experience from a reserved
airport-board application mode. Airport mode is not CLI-reachable and has no
provider or renderer. Its lightweight `AirportBoardState` contract is explicitly
separate from `FlightState`.

`VisualViewport` dispatches the implemented `AircraftVisual`,
`AltitudeProfileVisual`, and `RouteMapVisual`; radar and minimal remain internal,
CLI-inaccessible placeholders. All represent one `FlightState`; airport board
is not a visual mode.

```text
VisualViewport
├── AircraftVisual (implemented and default)
├── AltitudeProfileVisual (implemented and optional)
├── RouteMapVisual (implemented and optional)
├── RadarVisual (placeholder)
└── MinimalVisual (placeholder)
```

Route map is a small rendering subsystem. `map_geometry.c` samples the spherical
great-circle and fits an antimeridian-safe route-local projection into a bounded
`MapViewport`. The scene currently consists only of nominal route, endpoints,
validated progress position, and the nearby aircraft annotation. `map_raster.c`
feeds those same primitives to Braille or compatibility backends, with
`subcell_canvas.c` providing the reusable 2×4 Braille pixel grid. Future
geography, trails, waypoints, nearby traffic, weather, and airspace remain
separate optional scene layers rather than speculative `FlightState` fields.

`TelemetryHistory` is a provider-neutral bounded ring buffer of authoritative
observations accepted during the current process session. It belongs to one
resolved occurrence/leg, resets when that identity changes, and remains intact
when the visual changes. It is not a second source of current flight truth and
does not persist or backfill observations.

See `docs/future-features.md` for documentation-only directions. Remaining stub
visuals do not have public CLI controls and no visual makes provider calls.
