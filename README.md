# flight
<p align="center">
    <img width="908" height="550" alt="recording2" src="https://github.com/user-attachments/assets/d204df5e-9c08-4fff-b212-bf6d95729429" />
</p>


`flight` is a minimalist live flight tracker for macOS and Linux terminals.
Give it a commercial flight designator and it renders a responsive, animated
ASCII dashboard:

```sh
flight QF9
```

V0.1 resolves one coherent commercial occurrence and physical leg, optionally
joins it to fresh OpenSky telemetry by exact ICAO24, normalizes everything into
provider-neutral state, and renders it without a terminal UI framework.

## Requirements

- macOS or Linux
- a C11 compiler
- `make`
- libcurl development headers and library
- a POSIX-compatible terminal

On macOS, install the Xcode Command Line Tools if a compiler is missing. Common
Linux packages include `build-essential` and `libcurl4-openssl-dev` on
Debian/Ubuntu, or equivalent compiler and libcurl development packages.

## Build and test

```sh
make
make test
```

Strict warnings are enabled by default. The local executable is `./flight`.

An optional conventional installation target is available:

```sh
sudo make install
```

It installs to `/usr/local/bin` by default. A user-local installation can use:

```sh
PREFIX="$HOME/.local" make install
```

Ensure the selected `bin` directory is in `PATH` before using `flight QF9`.

## AirLabs credential

Live commercial-flight resolution requires an AirLabs API key:

```sh
export AIRLABS_API_KEY="your-key"
./flight BA281
```

`flight` uses a bring-your-own-key model. Every user should create an AirLabs
account and use the unique API key issued to that account. The project does not
include a shared or general-purpose key, and maintainers must not distribute
their personal key with source archives, binaries, packages, or installation
scripts.

The key is read from the environment when `flight` starts. It is never compiled
into the executable or copied by `make install`. For a single terminal session,
use the `export` command above. To load it in future zsh sessions on macOS, add
the following line to your private `~/.zshrc`, then open a new terminal or run
`source ~/.zshrc`:

```sh
export AIRLABS_API_KEY="your-own-key-here"
```

Other shells have equivalent private startup files. Confirm that a key is
available without displaying it:

```sh
if [ -n "$AIRLABS_API_KEY" ]; then
    echo "AirLabs key loaded"
else
    echo "AirLabs key missing"
fi
```

Never commit the key, paste it into issue reports or chats, or include it in
diagnostic logs. If a key is exposed, revoke or rotate it in the AirLabs account
dashboard. Local `.env` files are ignored as a precaution, but V0.1 does not
automatically load them; the shell environment remains the supported workflow.

Without a key, live AirLabs resolution reports that the credential is missing.
The application can still be built, tested, and run with the offline fixtures,
which make no network requests:

```sh
make test
./flight QF9 --fixture cruising
```

OpenSky telemetry is anonymous in V0.1. Its availability and licensing terms
are independent of AirLabs; confirm appropriate provider terms before using the
project commercially.

## Usage

```sh
./flight BA281
./flight QF9 --date YYYY-MM-DD
./flight QF1 --debug-provider
./flight QF9 --view aircraft
./flight --help
./flight --version
```

`--date` is a hard occurrence constraint, but AirLabs exposes only a limited
schedule window. `--debug-provider` prints credential-safe diagnostics after
the terminal UI closes. It never prints raw provider JSON or credentials.

Keyboard controls:

- `q` — quit
- `r` — refresh the active provider
- `f` — cycle fixtures in fixture mode only

## Offline fixtures

Fixtures are deterministic and make no network requests:

```sh
./flight QF9 --fixture cruising
./flight QF9 --fixture scheduled
./flight QF9 --fixture descending
./flight QF9 --fixture landed
./flight QF9 --fixture delayed
./flight QF9 --fixture stale
./flight QF9 --fixture unavailable
```

## Display-state semantics

- `LIVE` — exact ICAO24 match with telemetry whose provider timestamp is no more
  than 30 seconds old.
- `TRACKING` — the commercial occurrence is resolved, but current matching
  telemetry is unavailable.
- `STALE` — a matching observation exists but its provider timestamp is old.
- `SCHEDULED` — an upcoming or pre-departure occurrence is resolved and live
  telemetry is not expected.
- `OFFLINE` — no reliable commercial occurrence could be resolved.

OpenSky may have no matching state even when AirLabs resolves the correct
flight. The tracker then displays `TRACKING`, retains commercial metadata, and
leaves live telemetry fields unavailable.

## Flight-phase semantics

- `BOARDING` — pre-departure window with no airborne evidence.
- `SCHEDULED` — future scheduled occurrence.
- `TAXIING` — fresh on-ground telemetry above the taxi threshold.
- `CLIMBING`, `CRUISING`, `DESCENDING` — require fresh telemetry.
- `AIRBORNE` — provider confirms an active flight but precise motion telemetry
  is unavailable or stale.
- `LANDED` — provider status or actual-arrival evidence confirms completion.
- `DELAYED` and `UNAVAILABLE` — explicit provider/insufficient-data states.

Schedule timing alone never produces `CRUISING`.

## Progress semantics

Live geospatial progress is an along-track great-circle estimate using validated
airport and aircraft coordinates. Suspicious off-route positions fall back
safely. When live position is unavailable, progress may be estimated from
scheduled departure and arrival times.

Schedule-derived progress is a clock estimate, not the aircraft's geographic
position and not confirmation that it has landed.

## Responsive terminal behavior

- wide: 120 columns and above
- medium: 80–119 columns
- compact: 50–79 columns
- tiny: below 50 columns
- minimum supported terminal: 32×9

The interface handles live resizing, restores terminal state on exit, and uses
sparse heartbeat, propulsion, clock, freshness, and progress animation.

## Architecture

```text
AirLabs /flight + /schedules
        ↓
Validated/scored occurrence and physical leg
        ↓
ResolvedFlight ─────────────────────┐
                                    ├─→ Normalizer ─→ FlightState ─→ Renderer
OpenSkyTelemetry → TelemetrySnapshot┘

MockDataProvider ─────────────────────→ FlightState ─→ Renderer
```

Provider JSON never crosses the resolver/telemetry adapters. The renderer
consumes normalized `FlightState` only. Resolved occurrence metadata is cached
for six hours under `$XDG_CACHE_HOME/flight`, or `~/.cache/flight`; a bounded
stale metadata fallback may be used after resolver failure.

Airport codes and coordinates come from a generated static OurAirports snapshot
documented in `data/README.md`. Future visual and airport-mode contracts are
inactive placeholders and do not alter the V0.1 runtime.

## Known limitations

- OpenSky may have no current matching telemetry, especially across oceanic or
  sparse ADS-B coverage.
- AirLabs may omit ICAO24, aircraft model, registration, or other metadata.
- Without ICAO24, V0.1 does not attempt a loose callsign telemetry match.
- Some commercial-designator formats are unsupported; three-character airline
  prefixes are not automatically converted to IATA.
- Schedule progress is not geographic position and can conflict with delayed
  provider status near or after scheduled arrival.
- Provider-confirmed `AIRBORNE` is less precise than telemetry-derived phases.
- Airport reference data is a generated static snapshot and can become outdated.
- There is no historical telemetry backfill or runtime TelemetryHistory
  persistence/collection yet.
- Altitude profile, route map, radar, and minimal visual modes are internal
  placeholders only and are not CLI-reachable.
- Airport board mode is modeled separately but not implemented.

## Release and development documentation

- `docs/v0.1-validation.md` — V0.1 validation matrix
- `docs/releases/v0.1.0.md` — V0.1.0 release notes
- `docs/publishing.md` — credential-safe publishing and release checklist
- `docs/live-validation.md` — detailed live-provider validation notes
- `docs/future-features.md` — non-committed future directions
- `docs/architecture/` — documentation-only Archify diagrams

## License

Released under the [MIT License](LICENSE).
