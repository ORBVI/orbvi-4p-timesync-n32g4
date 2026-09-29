#ifndef TIME_PROTOCOL_H
#define TIME_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
} UtcTime;

bool utc_valid(const UtcTime *t);
void utc_next_second(UtcTime *t);
/* Exact ASCII command @UTC,YYYYMMDDhhmmss; without CR/LF. */
bool utc_parse_command(const char *line, size_t len, UtcTime *out);
/* RMC supports only UTC years 2000-2099. Returns bytes excluding NUL, or 0. */
size_t utc_format_rmc(const UtcTime *t, char *dst, size_t capacity);
#endif
