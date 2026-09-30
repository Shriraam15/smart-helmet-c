#ifndef HELMET_H
#define HELMET_H

/* Safety decision logic: ignition interlock, crash detection,
 * verdict classification, and runtime settings. */

#include "utils.h"
#include "sensors.h"

typedef enum helmet_verdict {
    VERDICT_OK = 0,          /* helmet worn, alcohol below limit        */
    VERDICT_NO_HELMET,
    VERDICT_DRUNK,
    VERDICT_CRASH            /* ride ended in a detected accident       */
} helmet_verdict;

typedef struct helmet_settings {
    double alcohol_limit_ppm;      /* interlock threshold              */
    double crash_threshold_g;      /* impact that counts as an accident */
    char   emergency_contact[PHONE_LEN];
    char   rider_name[NAME_LEN];
} helmet_settings;

typedef struct helmet_decision {
    helmet_bool ignition_allowed;
    helmet_verdict verdict;
    char reason[LINE_LEN];         /* human-readable explanation       */
} helmet_decision;

/* --------------------------------------------------------------- */
/* Settings: defaults + persistence in data/settings.txt            */
/* --------------------------------------------------------------- */

void        settings_default(helmet_settings *s);
/* Load overrides from SETTINGS_FILE; missing file = defaults.     */
void        settings_load(helmet_settings *s);
int         settings_save(const helmet_settings *s);

const char *verdict_name(helmet_verdict v);

/* --------------------------------------------------------------- */
/* Decisions                                                        */
/* --------------------------------------------------------------- */

/* Pre-ride ignition interlock check. */
helmet_decision helmet_check_interlock(const helmet_settings *cfg,
                                       int helmet_worn,
                                       double alcohol_ppm);

/* During-ride safety decision for one sensor tick.
 * active_riding: ride already started (no-helmet now = violation). */
helmet_decision helmet_evaluate_tick(const helmet_settings *cfg,
                                     const sensor_reading *r,
                                     int active_riding);

/* --------------------------------------------------------------- */
/* One full simulated ride                                          */
/* --------------------------------------------------------------- */

typedef struct ride_session {
    int    ride_id;
    char   rider_name[NAME_LEN];
    timestamp started_at;
    timestamp ended_at;

    double max_alcohol_ppm;
    double max_impact_g;
    int    helmet_was_removed;      /* helmet came off during ride     */
    int    drunk_events;            /* ticks with anomalous alcohol    */
    int    interlock_blocks;        /* start attempts refused          */
    int    crash_detected;

    helmet_verdict final_status;    /* COMPLETED / BLOCKED / ACCIDENT   */
    int    ticks_completed;
} ride_session;

void helmet_reset_ride(ride_session *ride, int ride_id,
                       const helmet_settings *cfg);

#endif /* HELMET_H */
