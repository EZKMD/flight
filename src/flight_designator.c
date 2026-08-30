#include "flight_designator.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

FlightDesignatorKind flight_designator_classify(const char *text)
{
    size_t length = strlen(text);
    size_t prefix = 0;
    size_t digits = 0;
    size_t index;
    if (length < 3 || length > 8) return DESIGNATOR_MALFORMED;
    while (prefix < length && prefix < 3 && isalnum((unsigned char)text[prefix])) {
        if (isdigit((unsigned char)text[prefix]) && prefix == 0) break;
        prefix++;
        if (prefix >= 2 && prefix < length && isdigit((unsigned char)text[prefix])) break;
    }
    if (prefix < 2 || prefix > 3) return DESIGNATOR_MALFORMED;
    index = prefix;
    while (index < length && isdigit((unsigned char)text[index]) && digits < 4) {
        index++;
        digits++;
    }
    if (digits == 0 || digits > 4) return DESIGNATOR_MALFORMED;
    if (index < length && isalpha((unsigned char)text[index])) index++;
    if (index != length) return DESIGNATOR_MALFORMED;
    return prefix == 2 ? DESIGNATOR_IATA_SUPPORTED : DESIGNATOR_PLAUSIBLE_UNSUPPORTED;
}
