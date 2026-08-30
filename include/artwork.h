#ifndef ARTWORK_H
#define ARTWORK_H

typedef struct {
    const char *const *lines;
    int line_count;
    int width;
} AircraftArtwork;

const AircraftArtwork *artwork_normal(void);
const AircraftArtwork *artwork_small(void);

#endif
