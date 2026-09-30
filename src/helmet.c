#include "helmet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/* Settings                                                           */
/* ------------------------------------------------------------------ */

void settings_default(helmet_settings *s)
{
    if (s == NULL) return;
    s->alcohol_limit_ppm  = 60.0;
    s->crash_threshold_g  = 4.0;
    strncpy(s->emergency_contact, "+91-9876543210", PHONE_LEN - 1);
    s->emergency_contact[PHONE_LEN - 1] = '\0';
    strncpy(s->rider_name, "Rider", NAME_LEN - 1);
    s->rider_name[NAME_LEN - 1] = '\0';
}

/* Trim trailing newline/CR/spaces so file-parsed values stay clean. */
static char *trim_tail(char *str)
{
    size_t n = strlen(str);
    while (n > 0 && (str[n-1] == '\n' || str[n-1] == '\r' ||
                     str[n-1] == ' '  || str[n-1] == '\t')) {
        str[--n] = '\0';
    }
    return str;
}

void settings_load(helmet_settings *s)
{
    FILE *fp;
    char line[LINE_LEN];
    char value[LINE_LEN];

    settings_default(s);
    fp = fopen(SETTINGS_FILE, "r");
    if (fp == NULL) {
        return;                          /* first run: keep defaults   */
    }

    while (fgets(line, sizeof line, fp) != NULL) {
        char *eq = strchr(line, '=');
        if (eq == NULL) continue;
        *eq = '\0';
        strncpy(value, eq + 1, sizeof value - 1);
        value[sizeof value - 1] = '\0';
        trim_tail(line);
        trim_tail(value);

        if (strcmp(line, "alcohol_limit_ppm") == 0) {
            s->alcohol_limit_ppm = atof(value);
        } else if (strcmp(line, "crash_threshold_g") == 0) {
            s->crash_threshold_g = atof(value);
        } else if (strcmp(line, "emergency_contact") == 0) {
            strncpy(s->emergency_contact, value, PHONE_LEN - 1);
            s->emergency_contact[PHONE_LEN - 1] = '\0';
        } else if (strcmp(line, "rider_name") == 0) {
            strncpy(s->rider_name, value, NAME_LEN - 1);
            s->rider_name[NAME_LEN - 1] = '\0';
        }
    }
    fclose(fp);
}

int settings_save(const helmet_settings *s)
{
    FILE *fp = fopen(SETTINGS_FILE, "w");
    int ok;

    if (fp == NULL) {
        return 0;
    }
    fprintf(fp, "alcohol_limit_ppm=%.2f\n", s->alcohol_limit_ppm);
    fprintf(fp, "crash_threshold_g=%.2f\n", s->crash_threshold_g);
    fprintf(fp, "emergency_contact=%s\n", s->emergency_contact);
    fprintf(fp, "rider_name=%s\n", s->rider_name);
    ok = (fflush(fp) == 0);
    fclose(fp);
    return ok;
}

const char *verdict_name(helmet_verdict v)
{
    switch (v) {
        case VERDICT_OK:         return "OK";
        case VERDICT_NO_HELMET:  return "NO HELMET";
        case VERDICT_DRUNK:      return "DRUNK";
        case VERDICT_CRASH:      return "CRASH";
    }
    return "UNKNOWN";
}

/* ------------------------------------------------------------------ */
/* Decisions                                                          */
/* ------------------------------------------------------------------ */

static helmet_decision make_decision(helmet_bool allowed,
                                     helmet_verdict verdict,
                                     const char *fmt, ...)
{
    helmet_decision d;
    va_list ap;

    d.ignition_allowed = allowed;
    d.verdict = verdict;

    va_start(ap, fmt);
    vsnprintf(d.reason, sizeof d.reason, fmt, ap);
    va_end(ap);
    return d;
}

helmet_decision helmet_check_interlock(const helmet_settings *cfg,
                                       int helmet_worn,
                                       double alcohol_ppm)
{
    if (!helmet_worn) {
        return make_decision(HELMET_FALSE, VERDICT_NO_HELMET,
                             "Helmet not worn - ignition locked.");
    }
    if (alcohol_ppm > cfg->alcohol_limit_ppm) {
        return make_decision(HELMET_FALSE, VERDICT_DRUNK,
                             "Alcohol %.0f ppm exceeds limit %.0f ppm.",
                             alcohol_ppm, cfg->alcohol_limit_ppm);
    }
    return make_decision(HELMET_TRUE, VERDICT_OK,
                         "Helmet worn, alcohol %.0f ppm (limit %.0f).",
                         alcohol_ppm, cfg->alcohol_limit_ppm);
}

helmet_decision helmet_evaluate_tick(const helmet_settings *cfg,
                                     const sensor_reading *r,
                                     int active_riding)
{
    if (r->impact_g >= cfg->crash_threshold_g) {
        return make_decision(HELMET_FALSE, VERDICT_CRASH,
                             "Impact %.1fg >= threshold %.1fg.",
                             r->impact_g, cfg->crash_threshold_g);
    }
    if (!r->helmet_worn) {
        return make_decision(HELMET_FALSE, VERDICT_NO_HELMET,
                             "Helmet removed while riding.");
    }
    if (r->alcohol_ppm > cfg->alcohol_limit_ppm) {
        return make_decision(HELMET_FALSE, VERDICT_DRUNK,
                             "Alcohol %.0f ppm exceeds limit %.0f ppm.",
                             r->alcohol_ppm, cfg->alcohol_limit_ppm);
    }
    if (active_riding) {
        return make_decision(HELMET_TRUE, VERDICT_OK,
                             "Tick %d: all sensors nominal.", r->tick_no);
    }
    return make_decision(HELMET_TRUE, VERDICT_OK, "Sensors nominal.");
}

/* ------------------------------------------------------------------ */
/* Ride session bookkeeping                                           */
/* ------------------------------------------------------------------ */

void helmet_reset_ride(ride_session *ride, int ride_id,
                       const helmet_settings *cfg)
{
    if (ride == NULL) return;
    memset(ride, 0, sizeof *ride);
    ride->ride_id      = ride_id;
    ride->final_status = VERDICT_OK;
    ride->max_alcohol_ppm = 0.0;
    ride->max_impact_g    = 0.0;
    if (cfg != NULL) {
        strncpy(ride->rider_name, cfg->rider_name, NAME_LEN - 1);
        ride->rider_name[NAME_LEN - 1] = '\0';
    }
}
