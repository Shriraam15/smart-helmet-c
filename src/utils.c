#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* xorshift64* PRNG: much better statistical quality than rand()      */
/* (MinGW's rand() is a 15-bit truncated LCG whose consecutive        */
/* outputs are correlated, which once blocked 6 demo rides in a row). */
/* ------------------------------------------------------------------ */

static unsigned long long g_rng_state = 0;
static int g_rng_seeded = 0;

static unsigned long long next_u64(void);

static void ensure_rng(void)
{
    if (!g_rng_seeded) {
        /* Mix clock, pointer ASLR entropy and counter into the seed. */
        unsigned long long seed = (unsigned long long)time(NULL) * 1000003ULL;
        seed ^= (unsigned long long)(uintptr_t)&seed;
        seed ^= (unsigned long long)clock();
        g_rng_state = seed | 1ULL;   /* never zero: xorshift trap state */
        /* warm up so weak low bits of the seed do not leak through */
        for (int i = 0; i < 8; i++) {
            next_u64();
        }
        g_rng_seeded = 1;
    }
}

static unsigned long long next_u64(void)
{
    unsigned long long x = g_rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    g_rng_state = x;
    return x * 2685821657736338717ULL;
}

double rand_double(double lo, double hi)
{
    ensure_rng();
    return lo + (hi - lo) * ((double)(next_u64() >> 11) / 9007199254740992.0);
}

int rand_int(int lo, int hi)
{
    ensure_rng();
    if (hi <= lo) {
        return lo;
    }
    return lo + (int)(next_u64() % (unsigned long long)(hi - lo + 1));
}

int chance(double probability)
{
    return rand_double(0.0, 1.0) < probability;
}

/* ------------------------------------------------------------------ */
/* Time helpers                                                       */
/* ------------------------------------------------------------------ */

timestamp now(void)
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    timestamp ts;

    if (lt == NULL) {
        memset(&ts, 0, sizeof ts);
        return ts;
    }
    ts.year   = lt->tm_year + 1900;
    ts.month  = lt->tm_mon + 1;
    ts.day    = lt->tm_mday;
    ts.hour   = lt->tm_hour;
    ts.minute = lt->tm_min;
    ts.second = lt->tm_sec;
    return ts;
}

void format_timestamp(timestamp ts, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%04d-%02d-%02d %02d:%02d:%02d",
             ts.year, ts.month, ts.day,
             ts.hour, ts.minute, ts.second);
}

void format_timestamp_compact(timestamp ts, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%04d-%02d-%02d_%02d-%02d-%02d",
             ts.year, ts.month, ts.day,
             ts.hour, ts.minute, ts.second);
}

double clamp_double(double v, double lo, double hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ------------------------------------------------------------------ */
/* Safe console input                                                 */
/* ------------------------------------------------------------------ */

static void flush_stdin_line(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        /* discard */
    }
}

int read_line(const char *prompt, char *buf, size_t bufsize)
{
    size_t len;

    if (buf == NULL || bufsize == 0) {
        return 0;
    }

    for (;;) {
        printf("%s", prompt);
        if (fgets(buf, (int)bufsize, stdin) == NULL) {
            return 0;                     /* EOF / stream error */
        }
        len = strlen(buf);
        if (len > 0 && buf[len - 1] != '\n') {
            flush_stdin_line();           /* line was too long: discard rest */
        }
        /* strip trailing newline / CR */
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[--len] = '\0';
        }
        return 1;
    }
}

/* Strict integer parse: whole line must be one integer, no extra junk. */
static int is_int_in_range(const char *s, int lo, int hi)
{
    char *end = NULL;
    long v;

    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return 0;
    v = strtol(s, &end, 10);
    if (end == s) return 0;
    while (isspace((unsigned char)*end)) end++;
    if (*end != '\0') return 0;
    return v >= lo && v <= hi;
}

/* Strict double parse: whole line must be one number, no extra junk. */
static int is_double_in_range(const char *s, double lo, double hi)
{
    char *end = NULL;
    double v;

    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return 0;
    v = strtod(s, &end);
    if (end == s) return 0;
    while (isspace((unsigned char)*end)) end++;
    if (*end != '\0') return 0;
    return v >= lo && v <= hi;
}

int read_int(const char *prompt, int lo, int hi)
{
    char line[LINE_LEN];

    for (;;) {
        if (!read_line(prompt, line, sizeof line)) {
            return lo;
        }
        if (is_int_in_range(line, lo, hi)) {
            return atoi(line);
        }
        printf("  ! Please enter a whole number between %d and %d.\n", lo, hi);
    }
}

double read_double(const char *prompt, double lo, double hi)
{
    char line[LINE_LEN];

    for (;;) {
        if (!read_line(prompt, line, sizeof line)) {
            return lo;
        }
        if (is_double_in_range(line, lo, hi)) {
            return atof(line);
        }
        printf("  ! Please enter a value between %.2f and %.2f.\n", lo, hi);
    }
}

void pause_console(void)
{
    char dummy[8];
    printf("\nPress Enter to continue...");
    fflush(stdout);
    read_line("", dummy, sizeof dummy);
}

/* ------------------------------------------------------------------ */
/* Box title                                                          */
/* ------------------------------------------------------------------ */

void print_title(const char *name, const char *version, int width)
{
    char header[LINE_LEN];

    snprintf(header, sizeof header, "%s v%s", name, version);

    int pad = width - (int)strlen(header) - 2;
    if (pad < 0) {
        pad = 0;
    }
    int left  = pad / 2;
    int right = pad - left;

    printf("\n");
    for (int i = 0; i < width; i++) putchar('=');
    printf("\n");
    printf(" %*s%s%*s\n", left, "", header, right, "");
    for (int i = 0; i < width; i++) putchar('=');
    printf("\n");
}
