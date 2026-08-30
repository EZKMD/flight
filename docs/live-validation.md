# Live provider validation

## First confirmed full live path: BA281

On 2026-08-30, BA281 was used as the first successful manual end-to-end live
validation. It is not a fixture and is not hard-coded into the application.

The test demonstrated:

- AirLabs commercial-occurrence resolution
- one coherent LHR / EGLL to LAX / KLAX physical leg
- exact ICAO24 joining to OpenSky
- fresh provider-timestamped telemetry
- the `LIVE` display state
- telemetry-derived `CRUISING`
- live great-circle progress
- aircraft metadata reaching the renderer

Observed aircraft data included Boeing 777-300ER, registration G-STBD, FL330,
490 KT, heading 328 degrees NW, and `DATA 13s AGO`.

## Manual live-flight checklist

Run each test manually to avoid consuming provider quota automatically:

```sh
./flight DESIGNATOR --debug-provider
```

Allow the dashboard to render, press `q`, and review the safe diagnostic output.

1. Current cruising flight: expect `LIVE`, fresh telemetry, `CRUISING`, and live
   geospatial progress.
2. Current climbing flight: expect `LIVE` and `CLIMBING`.
3. Current descending flight: expect `LIVE` and `DESCENDING`.
4. Upcoming flight: expect `SCHEDULED` and never a false `LIVE`.
5. Recently landed flight: expect `LANDED`, zero remaining time, and 100% progress.
6. Active occurrence without OpenSky telemetry: expect `TRACKING`, `AIRBORNE`,
   `NO TELEMETRY`, and schedule-time progress.
7. Unsupported or rejected designator: expect a specific resolver diagnostic,
   no crash, and no mock fallback.
8. Telemetry interruption: expect `LIVE` to become `STALE` while a recent last
   observation is retained, then `TRACKING` after retention expires; fresh
   telemetry must restore `LIVE`.

Never share API keys or raw provider responses when recording test results.
