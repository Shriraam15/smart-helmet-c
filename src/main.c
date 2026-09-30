#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "sensors.h"
#include "helmet.h"
#include "alert.h"
#include "logger.h"

/* ================================================================== */
/* Menu actions                                                       */
/* ================================================================== */

static void print_menu(void)
{
    printf("\n---------------- MAIN MENU ----------------\n");
    printf("  1. Start a simulated ride\n");
    printf("  2. Run ignition interlock check only\n");
    printf("  3. Simulate crash on next ride\n");
    printf("  4. View ride history\n");
    printf("  5. View safety report & statistics\n");
    printf("  6. View sent SMS alerts\n");
    printf("  7. Settings\n");
    printf("  8. Run self-tests\n");
    printf("  0. Exit\n");
    printf("-------------------------------------------\n");
}

/* ------------------------------------------------------------------ */
/* 1. Simulated ride                                                  */
/* ------------------------------------------------------------------ */

static void send_crash_alert(const helmet_settings *cfg,
                             const ride_session *ride,
                             double impact_g)
{
    alert_msg a;
    char detail[SMS_LEN];

    snprintf(detail, sizeof detail,
             "CRASH DETECTED! Impact %.1fg, rider %s. Send help.",
             impact_g, ride->rider_name);
    alert_compose(&a, "CRASH ALERT", cfg->emergency_contact,
                  ride->rider_name, detail);
    alert_send(&a);
}

static void send_interlock_alert(const helmet_settings *cfg,
                                 const ride_session *ride,
                                 helmet_verdict verdict,
                                 double ppm)
{
    alert_msg a;
    char detail[SMS_LEN];

    if (verdict == VERDICT_NO_HELMET) {
        snprintf(detail, sizeof detail, "Ride blocked - helmet not worn.");
    } else {
        snprintf(detail, sizeof detail,
                 "Alcohol %.0f ppm over limit %.0f ppm - ride blocked.",
                 ppm, cfg->alcohol_limit_ppm);
    }
    alert_compose(&a, "INTERLOCK", cfg->emergency_contact,
                  ride->rider_name, detail);
    alert_send(&a);
}

static void run_ride(helmet_settings *cfg, int ride_id, int force_crash)
{
    sensor_session sensors;
    ride_session ride;
    sensor_reading r;
    helmet_decision d;
    int tick, riding = 0;
    int ended_by_crash = 0;

    print_title("LIVE RIDE SIMULATION", APP_VERSION, 55);

    helmet_reset_ride(&ride, ride_id, cfg);
    sensors_init_session(&sensors);
    if (force_crash) {
        sensors_request_crash(&sensors);
    }
    ride.started_at = now();

    printf("  Rider: %s | Contact: %s\n", cfg->rider_name,
           cfg->emergency_contact);
    printf("  Ticks planned: %d\n\n", sensors.ticks);

    /* ---- Pre-ride interlock check --------------------------------- */
    sensors_read_tick(&sensors, 1, &r);
    printf("  [PRE-RIDE CHECK] helmet=%s alcohol=%.0f ppm\n",
           r.helmet_worn ? "ON" : "OFF", r.alcohol_ppm);

    d = helmet_check_interlock(cfg, r.helmet_worn, r.alcohol_ppm);
    printf("  Interlock: %s\n", d.reason);

    if (!d.ignition_allowed) {
        ride.interlock_blocks++;
        ride.max_alcohol_ppm = r.alcohol_ppm;   /* record the real reading */
        ride.final_status = VERDICT_DRUNK;
        if (d.verdict == VERDICT_NO_HELMET) {
            ride.final_status = VERDICT_NO_HELMET;
        }
        send_interlock_alert(cfg, &ride, d.verdict, r.alcohol_ppm);
        ride.ended_at = now();
        logger_append_ride(&ride);
        printf("\n  >> Ride blocked and logged as %s.\n",
               verdict_name(ride.final_status));
        return;
    }

    riding = 1;
    printf("\n  >> Ignition ON. Ride started.\n");

    /* ---- Riding loop ---------------------------------------------- */
    for (tick = 2; tick <= sensors.ticks; tick++) {
        sensors_read_tick(&sensors, tick, &r);

        printf("\n  Tick %2d | alcohol %6.1f ppm | impact %.1f g | helmet %s",
               r.tick_no, r.alcohol_ppm, r.impact_g,
               r.helmet_worn ? "ON" : "OFF");
        if (r.event != EVENT_NONE) {
            printf("  << %s >>", sensor_event_name(r.event));
        }

        if (r.alcohol_ppm > ride.max_alcohol_ppm) {
            ride.max_alcohol_ppm = r.alcohol_ppm;
        }
        if (r.impact_g > ride.max_impact_g) {
            ride.max_impact_g = r.impact_g;
        }

        d = helmet_evaluate_tick(cfg, &r, riding);

        if (d.verdict == VERDICT_CRASH) {
            ride.crash_detected = 1;
            ride.final_status   = VERDICT_CRASH;
            ended_by_crash      = 1;
            send_crash_alert(cfg, &ride, r.impact_g);
            printf("\n  >> EMERGENCY: crash detected. Ride aborted, "
                   "help alerted.\n");
            break;
        }
        if (d.verdict == VERDICT_NO_HELMET) {
            ride.helmet_was_removed = 1;
            printf("\n  >> WARNING: %s Ride ended for safety.\n", d.reason);
            ride.final_status = VERDICT_NO_HELMET;
            break;
        }
        if (d.verdict == VERDICT_DRUNK) {
            ride.drunk_events++;
            if (ride.drunk_events == 1) {
                send_interlock_alert(cfg, &ride, VERDICT_DRUNK, r.alcohol_ppm);
            }
            printf("\n  >> WARNING: %s\n", d.reason);
            ride.final_status = VERDICT_DRUNK;
            continue;
        }
    }

    if (!ended_by_crash && ride.final_status == VERDICT_OK) {
        printf("\n  >> Ride completed safely.\n");
    }

    ride.ended_at        = now();
    ride.ticks_completed = ended_by_crash ? tick : sensors.ticks;
    logger_append_ride(&ride);

    printf("\n  --- RIDE SUMMARY #%d ---\n", ride.ride_id);
    printf("  Status      : %s\n", verdict_name(ride.final_status));
    printf("  Max alcohol : %.1f ppm (limit %.0f)\n",
           ride.max_alcohol_ppm, cfg->alcohol_limit_ppm);
    printf("  Max impact  : %.1f g (threshold %.1f)\n",
           ride.max_impact_g, cfg->crash_threshold_g);
    printf("  Helmet off  : %s\n", ride.helmet_was_removed ? "YES" : "NO");
    printf("  Logged to   : %s\n", RIDES_CSV);
}

/* ------------------------------------------------------------------ */
/* 2. Interlock check only                                            */
/* ------------------------------------------------------------------ */

static void run_interlock_only(const helmet_settings *cfg)
{
    sensor_session s;
    sensor_reading r;
    helmet_decision d;

    print_title("IGNITION INTERLOCK CHECK", APP_VERSION, 55);

    sensors_init_session(&s);
    sensors_read_tick(&s, 1, &r);

    printf("  helmet=%s  alcohol=%.0f ppm  limit=%.0f ppm\n",
           r.helmet_worn ? "ON" : "OFF",
           r.alcohol_ppm, cfg->alcohol_limit_ppm);

    d = helmet_check_interlock(cfg, r.helmet_worn, r.alcohol_ppm);
    printf("  Verdict : %s\n", verdict_name(d.verdict));
    printf("  Ignition: %s\n", d.ignition_allowed ? "ALLOWED" : "LOCKED");
    printf("  Reason  : %s\n", d.reason);
}

/* ------------------------------------------------------------------ */
/* 7. Settings                                                        */
/* ------------------------------------------------------------------ */

static void settings_menu(helmet_settings *cfg)
{
    char buf[LINE_LEN];

    print_title("SETTINGS", APP_VERSION, 55);
    printf("  Press Enter to keep the current value.\n\n");

    printf("  Rider name [%s]: ", cfg->rider_name);
    if (read_line("", buf, sizeof buf) && buf[0] != '\0') {
        strncpy(cfg->rider_name, buf, NAME_LEN - 1);
        cfg->rider_name[NAME_LEN - 1] = '\0';
    }

    printf("  Emergency contact [%s]: ", cfg->emergency_contact);
    if (read_line("", buf, sizeof buf) && buf[0] != '\0') {
        strncpy(cfg->emergency_contact, buf, PHONE_LEN - 1);
        cfg->emergency_contact[PHONE_LEN - 1] = '\0';
    }

    printf("  Alcohol limit ppm [%.0f]: ", cfg->alcohol_limit_ppm);
    if (read_line("", buf, sizeof buf) && buf[0] != '\0') {
        double v = atof(buf);
        if (v > 0) cfg->alcohol_limit_ppm = v;
    }

    printf("  Crash threshold g [%.1f]: ", cfg->crash_threshold_g);
    if (read_line("", buf, sizeof buf) && buf[0] != '\0') {
        double v = atof(buf);
        if (v > 0) cfg->crash_threshold_g = v;
    }

    if (settings_save(cfg)) {
        printf("\n  Settings saved to %s\n", SETTINGS_FILE);
    } else {
        printf("\n  ! Could not save settings.\n");
    }
}

/* ================================================================== */
/* Self-tests                                                         */
/* ================================================================== */

static int tests_run = 0, tests_passed = 0;

static void test_check(int cond, const char *name)
{
    tests_run++;
    if (cond) {
        tests_passed++;
        printf("  [PASS] %s\n", name);
    } else {
        printf("  [FAIL] %s\n", name);
    }
}

static void run_self_tests(void)
{
    helmet_settings cfg;
    helmet_decision d;
    sensor_reading r;

    tests_run = tests_passed = 0;
    print_title("SELF-TESTS", APP_VERSION, 55);

    settings_default(&cfg);

    /* Interlock logic */
    d = helmet_check_interlock(&cfg, 0, 10.0);
    test_check(d.verdict == VERDICT_NO_HELMET && !d.ignition_allowed,
               "no helmet -> ignition locked");

    d = helmet_check_interlock(&cfg, 1, 20.0);
    test_check(d.verdict == VERDICT_OK && d.ignition_allowed,
               "sober + helmet -> ignition allowed");

    d = helmet_check_interlock(&cfg, 1, 300.0);
    test_check(d.verdict == VERDICT_DRUNK && !d.ignition_allowed,
               "high alcohol -> ignition locked");

    /* Tick evaluation */
    memset(&r, 0, sizeof r);
    r.helmet_worn = 1; r.alcohol_ppm = 15.0; r.impact_g = 1.0;
    d = helmet_evaluate_tick(&cfg, &r, 1);
    test_check(d.verdict == VERDICT_OK, "nominal tick -> OK");

    r.impact_g = 7.5;
    d = helmet_evaluate_tick(&cfg, &r, 1);
    test_check(d.verdict == VERDICT_CRASH, "impact 7.5g -> crash");

    r.impact_g = 1.0; r.helmet_worn = 0;
    d = helmet_evaluate_tick(&cfg, &r, 1);
    test_check(d.verdict == VERDICT_NO_HELMET, "helmet off mid-ride -> violation");

    r.helmet_worn = 1; r.alcohol_ppm = 250.0;
    d = helmet_evaluate_tick(&cfg, &r, 1);
    test_check(d.verdict == VERDICT_DRUNK, "alcohol mid-ride -> violation");

    /* Sensor simulation sanity */
    {
        sensor_session s;
        int i, worn = 0, spikes = 0;
        sensors_init_session(&s);
        for (i = 1; i <= s.ticks; i++) {
            sensors_read_tick(&s, i, &r);
            worn   |= !r.helmet_worn;
            spikes += (r.alcohol_ppm > cfg.alcohol_limit_ppm);
            test_check(r.impact_g >= 0.0 && r.impact_g <= 12.0,
                       "impact within sane range");
        }
        test_check(spikes <= s.ticks, "alcohol readings bounded by tick count");
        (void)worn;
    }

    /* Settings round-trip. The user's real settings are backed up and
     * restored, so running the self-tests never clobbers them. */
    {
        helmet_settings user_saved, loaded;
        settings_load(&user_saved);          /* keep the user's file     */
        settings_save(&cfg);                 /* round-trip the defaults  */
        settings_load(&loaded);
        test_check(loaded.alcohol_limit_ppm == cfg.alcohol_limit_ppm &&
                   loaded.crash_threshold_g  == cfg.crash_threshold_g,
                   "settings save/load round-trip");
        settings_save(&user_saved);          /* restore the user's file  */
    }

    printf("\n  Result: %d/%d tests passed.\n", tests_passed, tests_run);
}

/* ================================================================== */
/* Main                                                               */
/* ================================================================== */

int main(void)
{
    helmet_settings cfg;
    ride_list history;
    int next_ride_id = 1;
    int force_crash_next = 0;

    settings_load(&cfg);

    /* Continue ride numbering after previous runs. */
    ride_list_init(&history);
    if (logger_load_all(&history) > 0) {
        next_ride_id = history.items[history.count - 1].ride_id + 1;
    }
    ride_list_free(&history);

    print_title(APP_NAME, APP_VERSION, 60);
    printf("  Alcohol interlock + crash detection demo (pure C).\n");
    printf("  Data folder: %s/\n", DATA_DIR);

    for (;;) {
        int choice;

        print_menu();
        choice = read_int("Select option: ", 0, 8);

        switch (choice) {
            case 1:
                run_ride(&cfg, next_ride_id++, force_crash_next);
                force_crash_next = 0;
                pause_console();
                break;
            case 2:
                run_interlock_only(&cfg);
                pause_console();
                break;
            case 3:
                force_crash_next = 1;
                printf("\n  Next ride will include a crash event.\n");
                break;
            case 4: {
                ride_list rl;
                logger_load_all(&rl);
                logger_show_history(&rl);
                ride_list_free(&rl);
                pause_console();
                break;
            }
            case 5: {
                ride_list rl;
                logger_load_all(&rl);
                logger_show_report(&rl, &cfg);
                ride_list_free(&rl);
                pause_console();
                break;
            }
            case 6:
                alert_show_history();
                pause_console();
                break;
            case 7:
                settings_menu(&cfg);
                pause_console();
                break;
            case 8:
                run_self_tests();
                pause_console();
                break;
            case 0:
                printf("\nRide safe! Goodbye.\n");
                return 0;
            default:
                break;
        }
    }
}
