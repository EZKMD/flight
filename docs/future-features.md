# Future feature directions

This document preserves likely directions without committing them to a release
or implying that they are implemented. The validated v0.2.0 single-flight tracker
and its renderer remain the default product.

## Implemented through v0.2.0

- single commercial-flight lookup and occurrence selection
- normalized `FlightState` with explicit provenance
- AirLabs metadata plus exact-ICAO24 OpenSky telemetry
- responsive `AircraftVisual`
- live, tracking, stale, scheduled, and offline semantics
- deterministic mock fixtures and provider-safe diagnostics
- `VisualViewport` dispatch contract
- provider-neutral, session-only observed `TelemetryHistory`
- responsive `AltitudeProfileVisual` with CLI selection and runtime switching
- responsive `RouteMapVisual` with shared great-circle geometry, Braille and
  compatibility backends, optional Natural Earth coastline context, endpoints,
  directional annotation, and honest live-position semantics

## Candidate next work

- richer aircraft artwork selected by aircraft type and viewport tier

## Later possibilities

- `RadarVisual`, recent trail, and separately modeled nearby traffic
- explicit `MinimalVisual` distinct from responsive tiny layout
- airport arrivals/departures mode using `AirportBoardState`
- airport, origin, destination, and route weather
- gate, terminal, delay, cancellation, diversion, and route events
- timezone displays
- historical telemetry backfill and flight playback
- persisted telemetry history across tracker sessions
- saved/watchlisted flights and multiple tracked flights
- RouteMapVisual layers such as flown trail, waypoints, airports, pan/zoom,
  weather, nearby traffic, and airspace
- themes and configuration
- additional commercial-flight and historical-telemetry providers

## Architectural boundary

Aircraft, altitude profile, route map, radar, and minimal views are single-flight
visuals. Each consumes one normalized `FlightState` and optional
`TelemetryHistory` through `VisualViewport`. Route map V1 has a provider-neutral
viewport/projection, composable scene passes, and interchangeable raster
backends; geography, route, endpoint, position, and direction-annotation passes
exist today.

Airport board is a separate application mode representing many flights. It must
use a separate provider/state/renderer path and must not be inserted into
`FlightState` or treated as another `VisualMode`.

No future visual may call APIs, parse provider JSON, mutate `FlightState`, or own
network scheduling. Aircraft-specific art remains behind the visual/artwork
boundary and can later select model and size tiers without changing layout code.
