#ifndef ALERT_H
#define ALERT_H

/* Emergency SMS alert simulation: prints what a GSM module would
 * transmit and keeps a plain-text audit trail in ALERTS_LOG. */

#include "utils.h"

typedef struct alert_msg {
    timestamp at;
    char      to[PHONE_LEN];
    char      body[SMS_LEN + 1];
} alert_msg;

/* Print + append one alert. Returns 1 if the log write succeeded. */
int alert_send(const alert_msg *a);

/* Print every alert stored in ALERTS_LOG (newest last). */
void alert_show_history(void);

/* Compose a ready-to-send alert. type is short, e.g. "INTERLOCK",
 * "DRUNK RIDER", "CRASH ALERT". */
void alert_compose(alert_msg *a,
                   const char *type,
                   const char *contact,
                   const char *rider_name,
                   const char *detail);

#endif /* ALERT_H */
