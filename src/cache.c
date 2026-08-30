#include "cache.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CACHE_MAGIC UINT32_C(0x464c5433)
#define CACHE_VERSION UINT32_C(2)

typedef struct {
    uint32_t magic;
    uint32_t version;
    time_t stored_at;
    char query_flight[16];
    char query_date[11];
    ResolvedFlight resolved;
} CacheRecord;

static void sanitize(char *output, size_t capacity, const char *input)
{
    size_t index;
    for (index = 0; index + 1 < capacity && input[index] != '\0'; index++)
        output[index] = isalnum((unsigned char)input[index]) ? input[index] : '_';
    output[index] = '\0';
}

static bool cache_directory(char *directory, size_t capacity)
{
    const char *base = getenv("XDG_CACHE_HOME");
    const char *home = getenv("HOME");
    char parent[384];
    if (base == NULL || base[0] == '\0') {
        if (home == NULL || home[0] == '\0') return false;
        (void)snprintf(parent, sizeof(parent), "%s/.cache", home);
    } else (void)snprintf(parent, sizeof(parent), "%s", base);
    (void)mkdir(parent, 0700);
    (void)snprintf(directory, capacity, "%s/flight", parent);
    (void)mkdir(directory, 0700);
    return true;
}

static bool index_path(char *path, size_t capacity, const char *flight_number,
                       const char *date)
{
    char directory[416];
    char flight[32];
    char day[16];
    if (!cache_directory(directory, sizeof(directory))) return false;
    sanitize(flight, sizeof(flight), flight_number);
    sanitize(day, sizeof(day), date != NULL && date[0] != '\0' ? date : "current");
    return snprintf(path, capacity, "%s/%s-%s.index", directory, flight, day) > 0;
}

static bool record_path(char *path, size_t capacity, const ResolvedFlight *resolved)
{
    char directory[416];
    char occurrence[112];
    char leg[112];
    if (!cache_directory(directory, sizeof(directory))) return false;
    sanitize(occurrence, sizeof(occurrence), resolved->occurrence_id);
    sanitize(leg, sizeof(leg), resolved->selected_leg.leg_id);
    return snprintf(path, capacity, "%s/%s-%s.cache", directory, occurrence, leg) > 0;
}

bool cache_load_resolved(const char *flight_number, const char *date, time_t now,
                         long ttl_seconds, ResolvedFlight *resolved)
{
    char index_name[512];
    char record_name[512];
    char query_date[11];
    CacheRecord record;
    FILE *file;
    if (!index_path(index_name, sizeof(index_name), flight_number, date)) return false;
    file = fopen(index_name, "r");
    if (file == NULL) return false;
    if (fgets(record_name, sizeof(record_name), file) == NULL) {
        (void)fclose(file);
        return false;
    }
    (void)fclose(file);
    record_name[strcspn(record_name, "\r\n")] = '\0';
    file = fopen(record_name, "rb");
    if (file == NULL) return false;
    if (fread(&record, sizeof(record), 1, file) != 1) {
        (void)fclose(file);
        return false;
    }
    (void)fclose(file);
    (void)snprintf(query_date, sizeof(query_date), "%s", date != NULL ? date : "");
    if (record.magic != CACHE_MAGIC || record.version != CACHE_VERSION ||
        strcmp(record.query_flight, flight_number) != 0 ||
        strcmp(record.query_date, query_date) != 0 || now - record.stored_at < 0 ||
        now - record.stored_at > ttl_seconds) return false;
    *resolved = record.resolved;
    return true;
}

bool cache_store_resolved(const char *flight_number, const char *date,
                          const ResolvedFlight *resolved, time_t now)
{
    char path[512];
    char index_name[512];
    char temporary[544];
    char index_temporary[544];
    CacheRecord record;
    FILE *file;
    memset(&record, 0, sizeof(record));
    record.magic = CACHE_MAGIC;
    record.version = CACHE_VERSION;
    record.stored_at = now;
    record.resolved = *resolved;
    (void)snprintf(record.query_flight, sizeof(record.query_flight), "%s", flight_number);
    (void)snprintf(record.query_date, sizeof(record.query_date), "%s", date != NULL ? date : "");
    if (!record_path(path, sizeof(path), resolved) ||
        !index_path(index_name, sizeof(index_name), flight_number, date)) return false;
    (void)snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", path, (long)getpid());
    file = fopen(temporary, "wb");
    if (file == NULL) return false;
    if (fwrite(&record, sizeof(record), 1, file) != 1 || fclose(file) != 0 ||
        rename(temporary, path) != 0) {
        (void)unlink(temporary);
        return false;
    }
    (void)snprintf(index_temporary, sizeof(index_temporary), "%s.tmp.%ld", index_name,
                   (long)getpid());
    file = fopen(index_temporary, "w");
    if (file == NULL) return false;
    if (fprintf(file, "%s\n", path) < 0 || fclose(file) != 0 ||
        rename(index_temporary, index_name) != 0) {
        (void)unlink(index_temporary);
        return false;
    }
    return true;
}
