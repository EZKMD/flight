# Flight tracker architecture

These documentation artifacts describe the current C codebase. The source code remains the source of truth; regenerate the diagrams when the implementation changes.

## Artifacts

- `system-architecture.html` — provider orchestration, transport/cache support, normalization, `FlightState`, rendering, viewport, and terminal boundaries.
- `runtime-loops.html` — independent input, resize, animation, rendering, and provider-refresh schedules. It emphasizes that animation and rendering do not initiate API requests.
- `flight-lookup.html` — the actual `./flight QF9` startup sequence, occurrence selection, cached resolver metadata, OpenSky handoff, normalization, and rendering.
- `flight-state.html` — the logical areas of `FlightState`, their provider inputs, derived state, provenance, and renderer boundary.
- `flight-lifecycle.html` — provider- and telemetry-derived flight phases. This is derivation logic, not a persistent transition engine.
- `future-architecture.html` — implemented aircraft, altitude, and route visual
  paths, including the route map viewport/projection, composable V1 scene, and
  shared Braille/compatibility backend boundary.
- `ARCHITECTURE.md` — written details, renderer and repository maps, layout breakpoints, provider abstractions, and documentation-only future concepts.
- `specs/*.json` — Archify source specifications used to generate the standalone HTML files.

The generated HTML is self-contained and interactive. Delivery snapshots and visual-check evidence may be generated beside each HTML file by Archify.

## Regenerate

Archify is installed globally as a Codex skill, not as a project or C dependency. From the Archify skill directory, run:

```sh
node bin/archify.mjs deliver architecture /absolute/path/to/Flight/docs/architecture/specs/system-architecture.json /absolute/path/to/Flight/docs/architecture/system-architecture.html --quality showcase --json
node bin/archify.mjs deliver workflow /absolute/path/to/Flight/docs/architecture/specs/runtime-loops.json /absolute/path/to/Flight/docs/architecture/runtime-loops.html --quality showcase --json
node bin/archify.mjs deliver sequence /absolute/path/to/Flight/docs/architecture/specs/flight-lookup.json /absolute/path/to/Flight/docs/architecture/flight-lookup.html --quality showcase --json
node bin/archify.mjs deliver dataflow /absolute/path/to/Flight/docs/architecture/specs/flight-state.json /absolute/path/to/Flight/docs/architecture/flight-state.html --quality showcase --json
node bin/archify.mjs deliver lifecycle /absolute/path/to/Flight/docs/architecture/specs/flight-lifecycle.json /absolute/path/to/Flight/docs/architecture/flight-lifecycle.html --quality showcase --json
node bin/archify.mjs deliver architecture /absolute/path/to/Flight/docs/architecture/specs/future-architecture.json /absolute/path/to/Flight/docs/architecture/future-architecture.html --quality showcase --json
```

Replace `/absolute/path/to/Flight` with the repository path. Run `node bin/archify.mjs visual-check <html> --json` after delivery to capture viewport evidence.
