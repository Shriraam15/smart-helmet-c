#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Ride list helpers                                                  */
/* ------------------------------------------------------------------ */

void ride_list_init(ride_list *rl)
{
    if (rl == NULL) return;
    rl->items    = NULL;
    rl->count    = 0;
    rl->capacity = 0;
}

void ride_list_free(ride_list *rl)
{
    if (rl == NULL) return;
    free(rl->items);
    ride_list_init(rl);
}

static int ride_list_push(ride_list *rl, const ride_record *rec)
{
    if (rl->count == rl->capacity) {
        int newcap = (rl->capacity == 0) ? 8 : rl->capacity * 2;
        ride_record *tmp = realloc(rl->items, (size_t)newcap * sizeof *tmp);
        if (tmp == NULL) {
            return 0;
        }
        rl->items    = tmp;
        rl->capacity = newcap;
    }
    rl->items[rl->count++] = *rec;
    return 1;
}

/* ------------------------------------------------------------------ */
/* CSV output                                                         */
/* ------------------------------------------------------------------ */

int logger_append_ride(const ride_session *ride)
{
    FILE *fp;
    char start_s[LINE_LEN], end_s[LINE_LEN];
    int ok;

    if (ride == NULL) return 0;

    format_timestamp(ride->started_at, start_s, sizeof start_s);
    format_timestamp(ride->ended_at,   end_s,   sizeof end_s);

    fp = fopen(RIDES_CSV, "a");
    if (fp == NULL) {
        return 0;
    }
    fprintf(fp, "%d,%s,%s,%s,%.1f,%.1f,%d,%d,%d,%d,%s\n",
            ride->ride_id,
            ride->rider_name,
            start_s,
            end_s,
            ride->max_alcohol_ppm,
            ride->max_impact_g,
            ride->helmet_was_removed ? 1 : 0,
            ride->drunk_events,
            ride->interlock_blocks,
            ride->crash_detected ? 1 : 0,
            verdict_name(ride->final_status));
    ok = (fflush(fp) == 0);
    fclose(fp);
    return ok;
}

/* ------------------------------------------------------------------ */
/* CSV parsing                                                        */
/* ------------------------------------------------------------------ */

/* Split a CSV line into exactly 11 fields. Returns 1 on success. */
static int split_csv_line(char *line, char *fields[11])
{
    int n = 0;
    char *p = line;

    fields[n++] = p;
    while (*p != '\0' && n < 11) {
        if (*p == ',') {
            *p = '\0';
            fields[n++] = p + 1;
        }
        p++;
    }
    return (n == 11);
}

int logger_load_all(ride_list *rl)
{
    FILE *fp;
    char line[LINE_LEN * 2];

    ride_list_init(rl);

    fp = fopen(RIDES_CSV, "r");
    if (fp == NULL) {
        return 0;                    /* no history yet               */
    }

    while (fgets(line, sizeof line, fp) != NULL) {
        char *fields[11];
        ride_record rec;

        /* strip trailing newline */
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') {
            continue;                /* skip blank lines             */
        }

        if (!split_csv_line(line, fields)) {
            continue;                /* malformed line: skip it      */
        }

        memset(&rec, 0, sizeof rec);
        rec.ride_id            = atoi(fields[0]);
        strncpy(rec.rider_name, fields[1], NAME_LEN - 1);
        strncpy(rec.started,    fields[2], LINE_LEN - 1);
        strncpy(rec.ended,      fields[3], LINE_LEN - 1);
        rec.max_alcohol_ppm    = atof(fields[4]);
        rec.max_impact_g       = atof(fields[5]);
        rec.helmet_removed     = atoi(fields[6]);
        rec.drunk_events       = atoi(fields[7]);
        rec.interlock_blocks   = atoi(fields[8]);
        rec.crash_detected     = atoi(fields[9]);
        strncpy(rec.status,     fields[10], sizeof rec.status - 1);

        if (!ride_list_push(rl, &rec)) {
            fclose(fp);
            return rl->count;        /* out of memory: return partial */
        }
    }
    fclose(fp);
    return rl->count;
}

/* ------------------------------------------------------------------ */
/* History screen                                                     */
/* ------------------------------------------------------------------ */

static void print_history_header(void)
{
    printf("\n%-4s %-12s %-20s %-11s %-8s %-6s %-7s %-9s\n",
           "ID", "Rider", "Start", "MaxAlc", "MaxImp", "Alm", "Blk", "Status");
    printf("%.78s\n", "-----------------------------------------------------------------------------");
}

static void print_history_row(const ride_record *r)
{
    printf("%-4d %-12s %-20s %8.0fppm %5.1fg %5d %5d  %-9s\n",
           r->ride_id,
           r->rider_name,
           r->started,
           r->max_alcohol_ppm,
           r->max_impact_g,
           r->drunk_events,
           r->interlock_blocks,
           r->status);
}

void logger_show_history(const ride_list *rl)
{
    int i;

    if (rl->count == 0) {
        printf("\nNo rides logged yet. Start one from the main menu!\n");
        return;
    }

    print_history_header();
    for (i = 0; i < rl->count; i++) {
        print_history_row(&rl->items[i]);
    }
    printf("\n  %d ride(s) on record.\n", rl->count);
}

/* ------------------------------------------------------------------ */
/* Report / statistics                                                */
/* ------------------------------------------------------------------ */

static void report_status_line(const ride_list *rl, const char *status,
                               int *out_count)
{
    int i, n = 0;

    for (i = 0; i < rl->count; i++) {
        if (strcmp(rl->items[i].status, status) == 0) n++;
    }
    *out_count = n;
}

static void report_max_alcohol_day(const ride_list *rl, char *buf, size_t bufsize)
{
    double best = -1.0;
    int i;
    char day[16] = "-";

    for (i = 0; i < rl->count; i++) {
        if (rl->items[i].max_alcohol_ppm > best) {
            best = rl->items[i].max_alcohol_ppm;
            strncpy(day, rl->items[i].started, 10);
            day[10] = '\0';
        }
    }
    snprintf(buf, bufsize, "%s (%.0f ppm)", day, best < 0 ? 0.0 : best);
}

void logger_show_report(const ride_list *rl, const helmet_settings *cfg)
{
    int i;
    int completed = 0, helmet_stops = 0, blocked = 0, accidents = 0;
    int blocks_total = 0;
    double sum_alc = 0.0, max_alc = 0.0, sum_imp = 0.0, max_imp = 0.0;
    char day_buf[48];

    printf("\n==================== SAFETY REPORT ====================\n");

    if (rl->count == 0) {
        printf("  No ride data yet.\n");
        printf("=======================================================\n");
        return;
    }

    report_status_line(rl, "OK",        &completed);
    report_status_line(rl, "NO HELMET", &helmet_stops);
    report_status_line(rl, "DRUNK",     &blocked);
    report_status_line(rl, "CRASH",     &accidents);

    for (i = 0; i < rl->count; i++) {
        sum_alc += rl->items[i].max_alcohol_ppm;
        sum_imp += rl->items[i].max_impact_g;
        if (rl->items[i].max_alcohol_ppm > max_alc) max_alc = rl->items[i].max_alcohol_ppm;
        if (rl->items[i].max_impact_g    > max_imp) max_imp = rl->items[i].max_impact_g;
        blocks_total += rl->items[i].interlock_blocks;
    }

    printf("  Total rides             : %d\n", rl->count);
    printf("    - completed safely    : %d\n", completed);
    printf("    - stopped (helmet off): %d\n", helmet_stops);
    printf("    - interlock blocked   : %d\n", blocked);
    printf("    - accidents           : %d\n", accidents);
    printf("  Ignition block events   : %d\n", blocks_total);
    printf("  Rider                   : %s (contact %s)\n",
           cfg->rider_name, cfg->emergency_contact);
    printf("  Limits in force         : alcohol %.0f ppm, crash %.1f g\n",
           cfg->alcohol_limit_ppm, cfg->crash_threshold_g);
    printf("  Avg max alcohol         : %.1f ppm\n", sum_alc / rl->count);
    printf("  Peak alcohol            : %.0f ppm", max_alc);
    report_max_alcohol_day(rl, day_buf, sizeof day_buf);
    printf(" (peak day: %s)\n", day_buf);
    printf("  Avg max impact          : %.1f g\n", sum_imp / rl->count);
    printf("  Peak impact             : %.1f g\n", max_imp);
    printf("  Helmet removals         : ");
    {
        int rem = 0;
        for (i = 0; i < rl->count; i++) {
            rem += rl->items[i].helmet_removed ? 1 : 0;
        }
        printf("%d ride(s)\n", rem);
    }
    printf("=========================================================\n");
}
