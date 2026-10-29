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

typedef size_t (*Formatter)(const UtcTime *, char *, size_t);

static void check_sentence(Formatter format, UtcTime t, const char *expected)
{
    char sentence[UTC_SENTENCE_CAPACITY + 1u];
    size_t expected_len = strlen(expected);
    memset(sentence, '!', sizeof sentence);
    size_t len = format(&t, sentence, expected_len + 1u);
    assert(len == expected_len && strcmp(sentence, expected) == 0);
    assert(sentence[len + 1u] == '!');
    const char *star = strchr(sentence, '*');
    assert(star && (size_t)(star - sentence) == len - 5u);
    unsigned checksum = 0;
    for (const char *c = sentence + 1; c < star; ++c) checksum ^= (unsigned char)*c;
    unsigned encoded;
    assert(sscanf(star + 1, "%2X", &encoded) == 1 && checksum == encoded);
    memset(sentence, '!', sizeof sentence);
    assert(format(&t, sentence, expected_len) == 0);
    for (size_t i = 0; i < sizeof sentence; ++i) assert(sentence[i] == '!');
}

static void check_rmc(UtcTime t, const char *expected)
{
    check_sentence(utc_format_rmc, t, expected);
#if TIME_PROTOCOL == TIME_PROTOCOL_RMC
    check_sentence(utc_format_sentence, t, expected);
#endif
}

static void check_zda(UtcTime t, const char *expected)
{
    check_sentence(utc_format_zda, t, expected);
#if TIME_PROTOCOL == TIME_PROTOCOL_ZDA
    check_sentence(utc_format_sentence, t, expected);
#endif
}

static void emit_selected_sentences(void)
{
    /* Consecutive synthetic PPS cycles spanning leap-day midnight. */
    UtcTime t = {2024, 2, 29, 23, 59, 58};
    for (unsigned i = 0; i < 6; ++i) {
        char sentence[UTC_SENTENCE_CAPACITY];
        size_t len = utc_format_sentence(&t, sentence, sizeof sentence);
        assert(len && fwrite(sentence, 1, len, stdout) == len);
        utc_next_second(&t);
    }
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--emit") == 0) {
        emit_selected_sentences();
        return 0;
    }
    assert(argc == 1);
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
    check_zda((UtcTime){2025, 1, 1, 0, 0, 0}, "$GPZDA,000000.00,01,01,2025,00,00*63\r\n");
    check_zda((UtcTime){2000, 1, 1, 0, 0, 0}, "$GPZDA,000000.00,01,01,2000,00,00*64\r\n");
    check_zda((UtcTime){2024, 2, 29, 23, 59, 59}, "$GPZDA,235959.00,29,02,2024,00,00*6A\r\n");
    check_zda((UtcTime){2099, 12, 31, 23, 59, 59}, "$GPZDA,235959.00,31,12,2099,00,00*64\r\n");
    for (unsigned i = 0; i < 3; ++i) {
        Formatter format = (Formatter[]){utc_format_rmc, utc_format_zda, utc_format_sentence}[i];
        assert(!format(NULL, (char[UTC_SENTENCE_CAPACITY]){0}, UTC_SENTENCE_CAPACITY));
        assert(!format(&t, NULL, UTC_SENTENCE_CAPACITY));
        UtcTime invalid = {2023, 2, 29, 0, 0, 0};
        char untouched[UTC_SENTENCE_CAPACITY] = {'!'};
        assert(!format(&invalid, untouched, sizeof untouched) && untouched[0] == '!');
    }
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
    assert(!utc_format_zda(&t, sentence, sizeof sentence));
    assert(!utc_format_sentence(&t, sentence, sizeof sentence));
    assert(sentence[0] == '!'); /* No ambiguous ddmmyy=010100 after 2099. */
    puts("time protocol tests passed");
    return 0;
}
