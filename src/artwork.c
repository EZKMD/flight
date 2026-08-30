#include "artwork.h"

static const char *const normal_lines[] = {
    "       _      .---",
    "_______/ |__--'@/",
    "(______/__|__===-c"
};

static const char *const small_lines[] = {
    "|______.",
    "'-====---\""
};

static const AircraftArtwork normal = { normal_lines, 3, 19 };
static const AircraftArtwork small = { small_lines, 2, 10 };

const AircraftArtwork *artwork_normal(void) { return &normal; }
const AircraftArtwork *artwork_small(void) { return &small; }
