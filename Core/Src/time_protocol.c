#include "time_protocol.h"

static bool leap(uint16_t year)
{
    return (year % 4u == 0u && year % 100u != 0u) || year % 400u == 0u;
}

static uint8_t days(uint16_t year, uint8_t month)
{
    static const uint8_t days_per_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1u || month > 12u) return 0;
    return (month == 2u && leap(year)) ? 29u : days_per_month[month];
}

bool utc_valid(const UtcTime *t)
{
    return t && t->year >= 2000u && t->year <= 2099u && t->month >= 1u && t->month <= 12u &&
           t->day >= 1u && t->day <= days(t->year, t->month) && t->hour < 24u &&
           t->minute < 60u && t->second < 60u;
}

void utc_next_second(UtcTime *t)
{
    if (++t->second < 60u) return;
    t->second = 0;
    if (++t->minute < 60u) return;
    t->minute = 0;
    if (++t->hour < 24u) return;
    t->hour = 0;
    if (++t->day <= days(t->year, t->month)) return;
    t->day = 1;
    if (++t->month <= 12u) return;
    t->month = 1;
    ++t->year;
}

static bool two(const char *p, uint8_t *n)
{
    if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9') return false;
    *n = (uint8_t)((p[0] - '0') * 10 + p[1] - '0');
    return true;
}

bool utc_parse_command(const char *line, size_t len, UtcTime *out)
{
    UtcTime t;
    uint8_t century, year;
    if (!line || !out || len != 19u || line[0] != '@' || line[1] != 'U' ||
        line[2] != 'T' || line[3] != 'C' || line[4] != ',' ||
        !two(line+5, &century) || !two(line+7, &year) || century != 20u ||
        !two(line+9, &t.month) || !two(line+11, &t.day) || !two(line+13, &t.hour) ||
        !two(line+15, &t.minute) || !two(line+17, &t.second)) return false;
    t.year = (uint16_t)(century * 100u + year);
    if (!utc_valid(&t)) return false;
    *out = t;
    return true;
}

static void digits(char *dst, unsigned value, unsigned width)
{
    while (width) { dst[--width] = (char)('0' + value % 10u); value /= 10u; }
}

size_t utc_format_rmc(const UtcTime *t, char *dst, size_t capacity)
{
    /* $GPRMC,hhmmss.00,V,,,,,,,ddmmyy,,,N*XX\r\n : 40 bytes. */
    static const char template[] = "$GPRMC,000000.00,V,,,,,,,000000,,,N*00\r\n";
    if (!dst || capacity < sizeof template || !utc_valid(t)) return 0;
    for (size_t i = 0; i < sizeof template; ++i) dst[i] = template[i];
    digits(dst + 7, t->hour, 2); digits(dst + 9, t->minute, 2); digits(dst + 11, t->second, 2);
    digits(dst + 25, t->day, 2); digits(dst + 27, t->month, 2); digits(dst + 29, t->year % 100u, 2);
    uint8_t checksum = 0;
    size_t star = 1;
    while (dst[star] != '*') { checksum ^= (uint8_t)dst[star]; ++star; }
    static const char hex[] = "0123456789ABCDEF";
    dst[star + 1] = hex[checksum >> 4]; dst[star + 2] = hex[checksum & 15u];
    return sizeof(template) - 1u;
}
