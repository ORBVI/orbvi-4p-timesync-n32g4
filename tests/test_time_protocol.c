#include "time_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void parse(const char *s, unsigned year, unsigned month, unsigned day)
{
    UtcTime t = {0};
    assert(utc_parse_command(s, strlen(s), &t));
    assert(t.year == year && t.month == month && t.day == day);
}

static void check_rmc(UtcTime t, const char *expected)
{
    char sentence[41];
    size_t len = utc_format_rmc(&t, sentence, sizeof sentence);
    assert(len == 40u && strlen(expected) == len);
    assert(strcmp(sentence, expected) == 0);
    const char *star = strchr(sentence, '*');
    assert(star && star - sentence == 35);
    unsigned xor = 0;
    for (const char *p = sentence + 1; p < star; ++p) xor ^= (unsigned char)*p;
    unsigned encoded;
    assert(sscanf(star + 1, "%2X", &encoded) == 1 && xor == encoded);
    memset(sentence, '!', sizeof sentence);
    assert(utc_format_rmc(&t, sentence, 40u) == 0);
    assert(sentence[0] == '!');
}

int main(void)
{
    parse("@UTC,20240229235959", 2024, 2, 29);
    parse("@UTC,20000101000000", 2000, 1, 1);
    parse("@UTC,20991231235959", 2099, 12, 31);
    const char *bad[] = {"@UTC,20230229000000", "@UTC,19000101000000",
                         "@UTC,21000101000000", "@UTC,99990101000000",
                         "@UTC,20241301000000", "@UTC,20240101246000",
                         "@UTC,20240101235960", "@UTC,2024010123595x",
                         "@UTC,20240101235959\n", "@UTC,20240101235959garbage", "@UTC,2024010123595"};
    UtcTime t = {2024, 2, 29, 23, 59, 59};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        UtcTime copy = t;
        assert(!utc_parse_command(bad[i], strlen(bad[i]), &copy));
        assert(memcmp(&copy, &t, sizeof t) == 0);
    }
    check_rmc((UtcTime){2025, 1, 1, 0, 0, 0},
              "$GPRMC,000000.00,V,,,,,,,010125,,,N*7A\r\n");
    check_rmc((UtcTime){2000, 1, 1, 0, 0, 0},
              "$GPRMC,000000.00,V,,,,,,,010100,,,N*7D\r\n");
    check_rmc(t, "$GPRMC,235959.00,V,,,,,,,290224,,,N*73\r\n");
    check_rmc((UtcTime){2099, 12, 31, 23, 59, 59},
              "$GPRMC,235959.00,V,,,,,,,311299,,,N*7D\r\n");
    assert(!utc_format_rmc(NULL, (char[41]){0}, 41));
    assert(!utc_format_rmc(&t, NULL, 41));
    utc_next_second(&t);
    assert(t.year == 2024 && t.month == 3 && t.day == 1 && t.hour == 0 && t.minute == 0 && t.second == 0);
    t = (UtcTime){2023, 2, 28, 23, 59, 59}; utc_next_second(&t);
    assert(t.month == 3 && t.day == 1);
    t = (UtcTime){2025, 12, 31, 23, 59, 59}; utc_next_second(&t);
    assert(t.year == 2026 && t.month == 1 && t.day == 1);
    t = (UtcTime){2099, 12, 31, 23, 59, 59}; utc_next_second(&t);
    assert(t.year == 2100 && t.month == 1 && t.day == 1);
    char sentence[41] = {'!'};
    assert(!utc_valid(&t) && !utc_format_rmc(&t, sentence, sizeof sentence));
    assert(sentence[0] == '!'); /* No ambiguous ddmmyy=010100 after 2099. */
    puts("time protocol tests passed");
    return 0;
}
