# Airport reference snapshot

`airports_generated.inc` is a compact build-time snapshot of large airports with
scheduled service, IATA codes, and ICAO codes from the OurAirports
`airports.csv` dataset.

- Source: https://ourairports.com/data/
- Upstream repository: https://github.com/davidmegginson/ourairports-data
- Snapshot generated: 2026-08-30
- License: Public Domain

OurAirports states that all of its downloadable data is released to the Public
Domain and comes without a guarantee of accuracy or fitness for use. The source
CSV is not shipped because it is updated nightly and is substantially larger
than this project needs. Regenerate the snapshot with:

```sh
ruby scripts/generate_airports.rb airports.csv data/airports_generated.inc
```

The selected CSV does not include timezone identifiers, so generated records
leave timezone empty. The airport abstraction continues to degrade gracefully
for airports absent from this snapshot.
