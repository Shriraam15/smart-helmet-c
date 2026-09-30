#ifndef SENSORS_H
#define SENSORS_H

/* Simulated MQ-3 alcohol sensor, MPU-6050-style accelerometer,
 * and hall sensor for helmet-worn status. Auto-generates realistic
 * readings and randomly injects anomalies so demos stay lively. */

#include "utils.h"

typedef enum sensor_event_kind {
    EVENT_NONE = 0,
    EVENT_DRUNK_RIDER,       /* anomalous alcohol spike this tick        */
    EVENT_HELMET_REMOVED,    /* helmet strap sensor went open            */
    EVENT_CRASH              /* impact above threshold                   */
} sensor_event_kind;

typedef struct sensor_reading {
    int    tick_no;              /* 1..MAX_TICKS                        */
    double alcohol_ppm;          /* MQ-3 style ppm in the helmet cavity */
    double impact_g;             /* magnitude of acceleration in g      */
    int    helmet_worn;          /* 1 = strap closed                    */
    sensor_event_kind event;     /* anomaly injected this tick, if any  */
} sensor_reading;

typedef struct sensor_session {
    int    ticks;                /* how many readings this ride will have */
    int    crash_tick;           /* scheduled crash tick (0 = none)       */
    int    drunk_tick;           /* scheduled alcohol spike tick          */
    int    helmet_tick;          /* scheduled helmet-removal tick         */
    int    drunk_injected;       /* set once the anomaly has fired        */
    int    helmet_off_injected;
    int    crash_injected;
    int    crash_requested;      /* user forced a crash via the menu      */
} sensor_session;

void  sensors_init_session(sensor_session *s);
void  sensors_request_crash(sensor_session *s);

/* Generate the next simulated reading for the given tick. */
void  sensors_read_tick(sensor_session *s, int tick_no,
                        sensor_reading *out);

const char *sensor_event_name(sensor_event_kind ev);

#endif /* SENSORS_H */
