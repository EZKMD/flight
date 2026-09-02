# AirportBoard development mode

AirportBoard is a fixture-only development feature. It provides a live-style,
chronological airport activity board without credentials, network requests, or
live airport data. It is intentionally not advertised as a released live-data
feature yet.

Run it with:

```sh
./flight --airport MEL --fixture board
./flight --airport MEL --arrivals --fixture board
./flight --airport MEL --departures --fixture board
```

Only the synthetic MEL dataset is included in this phase. Departures are the
default direction.

## Interaction

- `↑` / `↓`: select the previous or next occurrence
- left click: select a visible occurrence
- `Enter`: open the selected occurrence in the existing flight tracker in the
  same process
- `a` / `d`: switch to arrivals or departures
- `r`: advance the deterministic fixture snapshot
- `b`: return from an opened flight to the preserved board
- `v` / `g`: retain normal flight-view behavior after opening a row
- `q`: quit

Arrivals and departures retain independent selection, scroll, and loaded-window
state. Mouse reporting is enabled only while the board is visible and is disabled
inside flight view and on exit. Terminal text selection may require the terminal's
usual modifier-key override while mouse reporting is active.

## Architecture

```text
Application
├── APP_MODE_FLIGHT
│   ├── FlightState
│   └── VisualViewport
└── APP_MODE_AIRPORT
    ├── AirportBoardState
    ├── AirportBoardStream[departures, arrivals]
    │   └── AirportFlightOccurrence[]
    ├── fixture temporal-window loader
    └── AirportBoardRenderer
```

`AirportBoardState` owns two dynamically allocated chronological streams. Each
stores loaded time bounds, earlier/later availability, a stable selected
`row_id`, and a scroll offset. Rows merge by `row_id`, grow dynamically, and
sort by original scheduled time. Estimates and actual times may change what is
displayed but never reorder the stream.

Selection is identity-based. Refresh retains the selected `row_id`; after a
tombstone retention period, a removed selection reconciles to the nearest
scheduled occurrence. Opening a row crosses a fixture translation boundary into
`FlightState`; the board remains alive for exact back navigation.

## Temporal model

The fixture contains five approximately six-hour windows: `earlier-2`,
`earlier-1`, `current`, `later-1`, and `later-2`. Only `current` loads initially.
Entering roughly the first or final 20 percent loads the adjacent window without
visible pagination or duplicates.

The production boundary uses half-open UTC windows, `[start, end)`. `NOW` is
derived during rendering and is never stored as an occurrence. MEL fixtures use
an explicit known `UTC+10` offset. Production airport timezone data remains a
provider/reference-data requirement; offsets must not be inferred from longitude.

## Contracts and honest semantics

`AirportFlightOccurrence` contains stable and optional provider identity,
identity confidence, operating and marketing designators, route and original
destination, scheduled/estimated/actual times, terminal, gate, belt, aircraft
metadata, freshness, cancellation/diversion flags, and timed change flags.

Normalized statuses are `UNKNOWN`, `EXPECTED`, `CHECK_IN`, `BOARDING`,
`GATE_CLOSED`, `DEPARTED`, `EN_ROUTE`, `APPROACHING`, `ARRIVED`, `DELAYED`,
`CANCELLED`, and `DIVERTED`. The board does not invent “final call,” “gate open,”
or “baggage ready.” Board freshness is independently `BOARD_LIVE`, `BOARD_STALE`,
or `BOARD_OFFLINE`.

Change flags cover gate, terminal, status, delay, belt, estimated time, newly
available actual time, and codeshares. Detection belongs to provider/fixture
state, not rendering. Flags carry detection and expiry times and settle after 15
seconds in this fixture.

## Rendering and refresh

Wide, medium, compact, and tiny layouts use discrete column or stacking choices.
Only status, activity, recent-change, and source indicators use restrained colour.
Text and symbols preserve meaning under `NO_COLOR`.

Rows stay calm except for low-frequency boarding, check-in, approaching, and
gate-closed activity. Recent changes are marked temporarily. Fixture refreshes
cycle through live changes, stale data, offline data, and recovery, exercising
gate, terminal, status, estimate/delay, belt, codeshare, actual-time, row-addition,
and tombstone/removal behavior.

## Future AeroDataBox boundary

No AeroDataBox code or networking exists in this phase. The next adapter belongs
upstream of the provider-neutral state:

```text
AirportBoardProvider
        ↓
AeroDataBoxProvider
        ↓
CodeshareGrouper
        ↓
IdentityReconciler
        ↓
AirportBoardState
```

It will supply normalized temporal windows and snapshots. State, merge, stable
selection, change-expiry, and rendering remain provider-neutral.
