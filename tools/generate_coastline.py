#!/usr/bin/env python3
"""Convert Natural Earth coastline GeoJSON into a compact C include.

Usage: tools/generate_coastline.py INPUT.geojson data/coastline_110m_generated.inc
"""

import hashlib
import json
import pathlib
import sys


def fail(message: str) -> None:
    raise SystemExit(message)


def main() -> None:
    if len(sys.argv) != 3:
        fail(f"usage: {sys.argv[0]} INPUT.geojson OUTPUT.inc")
    source = pathlib.Path(sys.argv[1])
    output = pathlib.Path(sys.argv[2])
    raw = source.read_bytes()
    document = json.loads(raw)
    if document.get("type") != "FeatureCollection":
        fail("expected a GeoJSON FeatureCollection")

    points: list[tuple[int, int]] = []
    offsets = [0]
    for feature in document.get("features", []):
        geometry = feature.get("geometry") or {}
        if geometry.get("type") != "LineString":
            fail("expected coastline LineString features only")
        previous = None
        for longitude, latitude, *_ in geometry.get("coordinates", []):
            point = (round(float(longitude) * 100), round(float(latitude) * 100))
            if point != previous:
                points.append(point)
                previous = point
        if offsets[-1] != len(points):
            offsets.append(len(points))

    if len(points) > 65535:
        fail("generated point array exceeds uint16_t offsets")
    output.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "/* Generated from Natural Earth 1:110m coastline. Do not edit. */",
        f"/* Source SHA-256: {hashlib.sha256(raw).hexdigest()} */",
        "static const CoastlinePoint coastline_points[] = {",
    ]
    for longitude, latitude in points:
        lines.append(f"    {{ {longitude}, {latitude} }},")
    lines.extend([
        "};",
        "",
        "static const uint16_t coastline_segment_offsets[] = {",
    ])
    for start in range(0, len(offsets), 12):
        group = ", ".join(str(value) for value in offsets[start:start + 12])
        lines.append(f"    {group},")
    lines.extend([
        "};",
        "",
        f"#define COASTLINE_POINT_COUNT {len(points)}U",
        f"#define COASTLINE_SEGMENT_COUNT {len(offsets) - 1}U",
        "",
    ])
    output.write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()
