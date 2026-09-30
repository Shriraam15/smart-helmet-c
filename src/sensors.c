#include "sensors.h"

#include <stdio.h>
#include <string.h>

/* Probability knobs for the demo experience. Each anomaly is decided
 * ONCE per ride (at init) and scheduled on a specific tick, so the
 * demo odds are predictable: with per-tick rolls, probabilities
 * compound across ticks and nearly every ride ended the same way. */
#define P_DRUNK_PER_RIDE     0.35   /* chance a ride has a drunk rider   */
#define P_HELMET_OFF         0.25   /* chance helmet comes off mid-ride  */
#define P_CRASH              0.12   /* chance of a random crash          */
#define P_DRUNK_PRERIDE      0.35   /* of drunk rides: blocked at start  */
#define FORCED_CRASH_TICK    3      /* guaranteed tick for forced crash  */

void sensors_init_session(sensor_session *s)
{
    if (s == NULL) return;
    memset(s, 0, sizeof *s);
    s->ticks = rand_int(8, MAX_TICKS);

    /* Schedule each anomaly at most once per ride. */
    if (chance(P_CRASH)) {
        s->crash_tick = rand_int(3, s->ticks);
    }
    if (chance(P_DRUNK_PER_RIDE)) {
        if (chance(P_DRUNK_PRERIDE)) {
            s->drunk_tick = 1;                 /* blocks ignition      */
        } else {
            s->drunk_tick = rand_int((s->ticks * 2) / 3 + 1, s->ticks);
        }
    }
    if (chance(P_HELMET_OFF)) {
        s->helmet_tick = rand_int(3, s->ticks);
    }
}

void sensors_request_crash(sensor_session *s)
{
    if (s == NULL) return;
    s->crash_requested = 1;
    /* A forced crash is for demos: guarantee it fires mid-ride and
     * that the pre-ride check is clean so the ride actually starts. */
    s->crash_tick    = FORCED_CRASH_TICK;
    s->drunk_tick    = 0;
    s->helmet_tick   = 0;
}

/* ------------------------------------------------------------------ */
/* Individual simulated sensors                                       */
/* ------------------------------------------------------------------ */

static double read_alcohol_ppm(int drunk_anomaly)
{
    if (drunk_anomaly) {
        /* Sharp elevated reading with human-like jitter. */
        return rand_double(ALCOHOL_DRUNK_MIN_PPM, ALCOHOL_DRUNK_MAX_PPM);
    }
    /* Sober baseline with mild sensor noise. */
    return ALCOHOL_BASELINE_PPM + rand_double(-6.0, 12.0);
}

static double read_impact_g(int crash_anomaly)
{
    if (crash_anomaly) {
        return rand_double(6.0, 9.5);          /* hard impact        */
    }
    return IMPACT_IDLE_G + rand_double(0.0, 0.6); /* road vibration */
}

/* ------------------------------------------------------------------ */
/* Which scheduled anomaly (if any) fires on this tick                */
/* ------------------------------------------------------------------ */

static sensor_event_kind pick_anomaly(const sensor_session *s, int tick_no)
{
    if (s->crash_tick == tick_no) {
        return EVENT_CRASH;
    }
    if (s->drunk_tick == tick_no) {
        return EVENT_DRUNK_RIDER;
    }
    if (s->helmet_tick == tick_no) {
        return EVENT_HELMET_REMOVED;
    }
    return EVENT_NONE;
}

void sensors_read_tick(sensor_session *s, int tick_no, sensor_reading *out)
{
    sensor_event_kind ev;

    if (s == NULL || out == NULL) return;

    ev = pick_anomaly(s, tick_no);

    /* Anomalies persist for the rest of the ride via session flags:
     * once drunk, stays drunk; once the helmet is off, it stays off. */
    if (tick_no >= s->drunk_tick && s->drunk_tick > 0) {
        s->drunk_injected = 1;
    }
    if (tick_no >= s->helmet_tick && s->helmet_tick > 0) {
        s->helmet_off_injected = 1;
    }
    if (ev == EVENT_CRASH) {
        s->crash_injected = 1;
    }

    out->tick_no = tick_no;
    out->event   = ev;
    out->alcohol_ppm = read_alcohol_ppm(s->drunk_injected);
    out->impact_g    = read_impact_g(ev == EVENT_CRASH);
    out->helmet_worn = !s->helmet_off_injected;
}

const char *sensor_event_name(sensor_event_kind ev)
{
    switch (ev) {
        case EVENT_DRUNK_RIDER:      return "Alcohol spike";
        case EVENT_HELMET_REMOVED:   return "Helmet removed";
        case EVENT_CRASH:            return "Impact";
        case EVENT_NONE:             return "-";
    }
    return "-";
}
