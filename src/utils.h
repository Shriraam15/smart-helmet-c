#ifndef UTILS_H
#define UTILS_H

/* Shared constants, small helpers, and safe console input.
 * Everything here is plain C99 - no platform-specific calls. */

#include <stddef.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Project-wide constants                                             */
/* ------------------------------------------------------------------ */

#define APP_NAME        "Smart Helmet Safety System"
#define APP_VERSION     "1.0"

#define DATA_DIR        "data"
#define RIDES_CSV       DATA_DIR "/rides.csv"
#define ALERTS_LOG      DATA_DIR "/alerts.log"
#define SETTINGS_FILE   DATA_DIR "/settings.txt"

#define MAX_RIDERS      8            /* rider profiles stored in memory  */
#define NAME_LEN        32           /* max chars in a rider name        */
#define PHONE_LEN       16           /* max chars in a phone number      */
#define LINE_LEN        256          /* general text buffer size         */
#define SMS_LEN         160          /* classic single SMS length        */
#define MAX_TICKS       24           /* max sensor ticks per ride        */

#define ALCOHOL_BASELINE_PPM   18.0  /* sober air ppm around the rider   */
#define ALCOHOL_DRUNK_MIN_PPM  150.0 /* injected "drunk" anomaly range   */
#define ALCOHOL_DRUNK_MAX_PPM  400.0

#define IMPACT_IDLE_G          1.0   /* normal gravity / road vibration  */

/* ------------------------------------------------------------------ */
/* Simple types                                                       */
/* ------------------------------------------------------------------ */

typedef enum helmet_bool {
    HELMET_FALSE = 0,
    HELMET_TRUE  = 1
} helmet_bool;

typedef struct timestamp {
    int year, month, day;
    int hour, minute, second;
} timestamp;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Seed-safe uniform double in [lo, hi]. */
double rand_double(double lo, double hi);

/* Uniform integer in [lo, hi] inclusive. */
int rand_int(int lo, int hi);

/* True with the given probability (0.0 - 1.0). */
int chance(double probability);

/* Current local wall-clock time. */
timestamp now(void);

/* "2026-09-30 14:05:09" */
void format_timestamp(timestamp ts, char *buf, size_t bufsize);

/* Compact form used inside CSV/LOG lines: "2026-09-30_14-05-09" */
void format_timestamp_compact(timestamp ts, char *buf, size_t bufsize);

/* Clamp v into [lo, hi]. */
double clamp_double(double v, double lo, double hi);

/* ------------------------------------------------------------------ */
/* Safe console input (tolerates trailing junk, no scanf pitfalls)    */
/* ------------------------------------------------------------------ */

/* Print prompt then read a line; returns 1 on success. */
int read_line(const char *prompt, char *buf, size_t bufsize);

/* Print prompt and read an integer; keeps asking until valid. */
int read_int(const char *prompt, int lo, int hi);

/* Print prompt and read a double; keeps asking until valid. */
double read_double(const char *prompt, double lo, double hi);

/* Pause until the user presses Enter. */
void pause_console(void);

/* Print "name (v)" centered inside a box of total width w. */
void print_title(const char *name, const char *version, int width);

#endif /* UTILS_H */
