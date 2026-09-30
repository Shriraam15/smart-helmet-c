#ifndef LOGGER_H
#define LOGGER_H

/* Ride history: append to RIDES_CSV, read back for the history
 * screen, and aggregate simple statistics for the report screen. */

#include "helmet.h"

typedef struct ride_record {
    int        ride_id;
    char       rider_name[NAME_LEN];
    char       started[LINE_LEN];   /* formatted timestamps           */
    char       ended[LINE_LEN];
    double     max_alcohol_ppm;
    double     max_impact_g;
    helmet_bool helmet_removed;
    int        drunk_events;
    int        interlock_blocks;
    helmet_bool crash_detected;
    char       status[32];          /* COMPLETED / BLOCKED / ACCIDENT */
} ride_record;

/* Append one ride to the CSV. Returns 1 on success. */
int logger_append_ride(const ride_session *ride);

/* Dynamic list of ride records parsed from the CSV. */
typedef struct ride_list {
    ride_record *items;
    int          count;
    int          capacity;
} ride_list;

void ride_list_init(ride_list *rl);
void ride_list_free(ride_list *rl);

/* Returns number of records loaded (0 if the file doesn't exist). */
int  logger_load_all(ride_list *rl);

/* Print the full history table. */
void logger_show_history(const ride_list *rl);

/* Print aggregate statistics: rides per day, totals, averages. */
void logger_show_report(const ride_list *rl, const helmet_settings *cfg);

#endif /* LOGGER_H */
