# Future feature directions

This document preserves likely directions without committing them to a release
or implying that they are implemented. The validated V0.1 aircraft tracker and
its renderer remain the default product.

## Implemented in V0.1

- single commercial-flight lookup and occurrence selection
- normalized `FlightState` with explicit provenance
- AirLabs metadata plus exact-ICAO24 OpenSky telemetry
- responsive `AircraftVisual`
- live, tracking, stale, scheduled, and offline semantics
- deterministic mock fixtures and provider-safe diagnostics
- `VisualViewport` dispatch contract
- inactive provider-neutral `TelemetryHistory` model

## Candidate next work

- `AltitudeProfileVisual` grown from telemetry collected after startup
- `RouteMapVisual` with origin, destination, great-circle route, and aircraft
- optional visual selection and cycling after the views are implemented
- richer aircraft artwork selected by aircraft type and viewport tier

## Later possibilities

- `RadarVisual`, recent trail, and separately modeled nearby traffic
- explicit `MinimalVisual` distinct from responsive tiny layout
- airport arrivals/departures mode using `AirportBoardState`
- airport, origin, destination, and route weather
- gate, terminal, delay, cancellation, diversion, and route events
- timezone displays
- historical telemetry backfill and flight playback
- saved/watchlisted flights and multiple tracked flights
- simplified world outlines, map layers, pan/zoom, and waypoints
- themes and configuration
- additional commercial-flight and historical-telemetry providers

## Architectural boundary

Aircraft, altitude profile, route map, radar, and minimal views are single-flight
visuals. Each consumes one normalized `FlightState` and optional
`TelemetryHistory` through `VisualViewport`.

Airport board is a separate application mode representing many flights. It must
use a separate provider/state/renderer path and must not be inserted into
`FlightState` or treated as another `VisualMode`.

No future visual may call APIs, parse provider JSON, mutate `FlightState`, or own
network scheduling. Aircraft-specific art remains behind the visual/artwork
boundary and can later select model and size tiers without changing layout code.
