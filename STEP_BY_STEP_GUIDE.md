# Step-by-Step Guide — Understand, Rebuild, Explain

This is your personal study guide. Work through it in order; each step ends
with something you can *do*, not just read. Goal: you can reproduce and
defend this project without AI in the room.

---

## Part A — Understand the big picture (30 min)

### A1. The one-sentence version
> A rider can only start the bike if the helmet is on and they are sober;
> while riding, a crash or helmet removal triggers an emergency alert.

Everything in ~1,100 lines of C and one .ino file is *just* this sentence,
made precise.

### A2. Draw this on paper from memory
```
[MQ-3 gas]  [MPU-6050]  [strap switch]      ← sensors
      \         |          /
       ─────────────────────
       │  DECISION LOGIC  │                ← helmet.c (interlock, verdicts)
       ─────────────────────
        │        │         │
   [ignition] [buzzer/SMS] [ride log]      ← alert.c / logger.c
```
If you can redraw this box diagram and name each arrow's data, you already
understand 60% of the project.

### A3. The three decisions the system makes
1. **Pre-ride:** start allowed? (helmet worn AND alcohol < limit)
2. **Per tick:** continue riding? (no crash AND helmet on AND sober)
3. **On event:** which alert? (INTERLOCK vs CRASH, who gets told, what is logged)

---

## Part B — Walk the actual code (90 min)

Open VS Code and follow along; the order below is easiest → hardest.

### B1. `utils.h` / `utils.c` — the toolbox
Read `rand_double`, `chance`, `now`, `format_timestamp`, `read_int`.

- `chance(0.35)` = "35% likely". One line of code — find it.
- `read_int` uses `strtol` + checks the *whole* line was consumed.
  Viva trap: why not `scanf("%d", &x)`? Because "12abc" would be accepted
  as 12, letters left in the buffer poison the next read, and there is no
  range check. Try it: feed "12abc" to a `scanf` version and watch.

**Do:** change `chance()` to print every random draw for one ride; see how
often each probability wins.

### B2. `sensors.c` — the fake world
- Each anomaly is **scheduled once per ride** in `sensors_init_session`
  (crash tick, drunk tick, helmet tick). Find the three `rand_int` calls.
- `sensors_read_tick` then just *reports* what is scheduled — once drunk,
  stays drunk (flags `drunk_injected`, `helmet_off_injected`).

**Do:** raise `P_CRASH` to 0.5, rebuild, run 10 rides, count statuses
(menu 4). This is you *doing an experiment* on your own system.

### B3. `helmet.c` — the judge
`helmet_check_interlock` is the whole safety policy in ~15 lines:
helmet first, then alcohol, then OK. Each outcome returns a
`helmet_decision` struct: allowed? verdict enum? human reason string?

Viva gold: *why return a struct instead of printing?* Because decisions and
presentation are separated — the same decision can go to the console, a CSV
row, or an SMS without changing the logic. That sentence shows design
maturity beyond year-2 level (it is the same idea as your Recover AI
"safety gate" — deterministic rules before anything else acts).

### B4. `main.c` `run_ride` — the conductor
Trace one ride line by line: reset → pre-ride check → either blocked
(log + SMS + return) or riding loop → per tick: read, update maxima,
evaluate, handle verdict → summary → `logger_append_ride`.

**Do:** predict what happens if `sensors.ticks` is 2 and a helmet-off is
scheduled at tick 3 — then make it happen (set `s->ticks = 2` temporarily).

### B5. `logger.c` + `alert.c` — memory
- CSV row = one ride; `split_csv_line` cuts on commas (count: 11 fields).
- The report aggregates with plain loops — status counts are string
  compares against the status column.

**Do:** open `data/rides.csv` in Excel after 10 rides and verify the
report's "accidents" count against column K by hand. Then you have
*personally validated* your own logging pipeline.

---

## Part C — The Wokwi version (60 min)

Follow `wokwi/WOKWI_GUIDE.md` end to end. Then answer, without looking:

1. Why does the LCD share pins A4/A5 with the MPU6050 without conflict?
2. Why `INPUT_PULLUP` on the strap button, and why does pressed read LOW?
3. Why subtract 1 g from the accelerometer magnitude?
4. Why fail loudly when `mpu.begin()` fails?

If you can answer all four in your own words, the embedded side is yours.

---

## Part D — Rebuild it yourself (the real test)

On a blank folder, *without copy-paste*, recreate this exact minimal
version (~120 lines, 2 files). Not the whole project — the skeleton that
proves understanding:

**interlock.h**
```c
#ifndef INTERLOCK_H
#define INTERLOCK_H
typedef struct { int helmet_worn; double alcohol_ppm; } reading;
typedef struct { int allowed; const char *reason; } decision;
decision check(const reading *r, double limit);
#endif
```

**interlock.c + main.c**: implement `check` (helmet first, then alcohol),
then in `main` ask the user for the two values and print ALLOWED or LOCKED
with the reason. Build it, run it, break it, fix it.

When this compiles and behaves identically to the real one for all three
outcomes, you have internalized the core. Everything else (SMS, CSV,
report) is *additive plumbing* you already have as reference code.

---

## Part E — Explain it (last 30 min)

Say each answer out loud, alone, in one breath:

1. "The helmet continuously monitors alcohol, strap state and impact;
   ignition is only unlocked when helmet is worn and alcohol is under
   the legal limit; a crash or removal mid-ride aborts the ride and
   alerts the emergency contact."
2. "I separated sensing, deciding, alerting and logging into modules so
   each can be tested and replaced alone — the simulator can swap for
   real sensors without touching the policy code."
3. "I found and fixed five bugs during testing: a forced crash that could
   fire before ignition, blocked rides logging zero alcohol, a wrong SMS
   on helmet blocks, self-tests overwriting user settings, and a Windows
   rand() correlation that biased the demo odds."
4. "On Wokwi the same policy runs on an Arduino with an MQ-2, MPU6050,
   I2C LCD and a strap button — proving the logic is hardware-independent."

Number 3 is your superpower in the viva: **examiners reward students who
found and fixed their own bugs** far more than flawless demos.

---

## Common mistakes to never make again
- `scanf("%d")` for untrusted input — use `strtol`/`strtod` + full-line check
- Trusting `rand()` quality on Windows (15-bit LCG, correlated) — xorshift64*
- Per-ticket probability rolls that compound — schedule once per session
- Global state before a decision — pass a `const struct *` and return a result
- Printing inside decision functions — return data, let callers present it

## When you're stuck in the exam/lab
Read the compiler warning *literally*. Fix the first error, not the last.
Rebuild after every small change. If a value surprises you, print it at
every step until it doesn't.
