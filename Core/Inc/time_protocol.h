#ifndef TIME_PROTOCOL_H
#define TIME_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TIME_PROTOCOL_ZDA 1
#define TIME_PROTOCOL_RMC 2
#ifndef TIME_PROTOCOL
#define TIME_PROTOCOL TIME_PROTOCOL_ZDA
#endif
#if TIME_PROTOCOL != TIME_PROTOCOL_ZDA && TIME_PROTOCOL != TIME_PROTOCOL_RMC
#error "TIME_PROTOCOL must be TIME_PROTOCOL_ZDA or TIME_PROTOCOL_RMC"
#endif
/* Largest supported sentence, including its terminating NUL. */
#define UTC_SENTENCE_CAPACITY 41u

typedef struct {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
} UtcTime;

bool utc_valid(const UtcTime *t);
void utc_next_second(UtcTime *t);
/* Exact ASCII command @UTC,YYYYMMDDhhmmss; without CR/LF. */
bool utc_parse_command(const char *line, size_t len, UtcTime *out);
/* All formatters support UTC years 2000-2099 and return bytes excluding NUL, or 0. */
size_t utc_format_zda(const UtcTime *t, char *dst, size_t capacity);
/* Legacy simulated no-fix RMC (V/N); not a valid GNSS fix. */
size_t utc_format_rmc(const UtcTime *t, char *dst, size_t capacity);
/* Exactly one sentence, selected at compile time; ZDA is the default. */
size_t utc_format_sentence(const UtcTime *t, char *dst, size_t capacity);
#endif
