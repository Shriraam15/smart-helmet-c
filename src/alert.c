#include "alert.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Compose                                                            */
/* ------------------------------------------------------------------ */

void alert_compose(alert_msg *a,
                   const char *type,
                   const char *contact,
                   const char *rider_name,
                   const char *detail)
{
    if (a == NULL) return;

    a->at = now();
    strncpy(a->to, contact, PHONE_LEN - 1);
    a->to[PHONE_LEN - 1] = '\0';
    snprintf(a->body, sizeof a->body, "[%s] %s: %s", type, rider_name, detail);
}

/* ------------------------------------------------------------------ */
/* Send (print + log)                                                 */
/* ------------------------------------------------------------------ */

int alert_send(const alert_msg *a)
{
    FILE *fp;
    char ts[LINE_LEN];

    if (a == NULL) return 0;

    format_timestamp(a->at, ts, sizeof ts);

    printf("\n  +----------------------------------------+\n");
    printf("  |           SMS ALERT TRANSMITTED        |\n");
    printf("  +----------------------------------------+\n");
    printf("  To      : %s\n", a->to);
    printf("  Time    : %s\n", ts);
    printf("  Message : %s\n", a->body);
    printf("  +----------------------------------------+\n");

    fp = fopen(ALERTS_LOG, "a");
    if (fp == NULL) {
        printf("  ! Could not write %s\n", ALERTS_LOG);
        return 0;
    }
    fprintf(fp, "%s | to=%s | %s\n", ts, a->to, a->body);
    fclose(fp);
    return 1;
}

void alert_show_history(void)
{
    FILE *fp;
    char line[LINE_LEN];
    int count = 0;

    printf("\n--- SMS ALERT HISTORY (%s) ---\n", ALERTS_LOG);

    fp = fopen(ALERTS_LOG, "r");
    if (fp == NULL) {
        printf("  No alerts sent yet.\n");
        return;
    }
    while (fgets(line, sizeof line, fp) != NULL) {
        fputs("  ", stdout);
        fputs(line, stdout);
        count++;
    }
    fclose(fp);

    if (count == 0) {
        printf("  No alerts sent yet.\n");
    } else {
        printf("  Total: %d alert(s)\n", count);
    }
}
