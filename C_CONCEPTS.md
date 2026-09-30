# C Concepts Used — Where and How

A concept-by-concept map of the C features in `src/`, with the exact place
each is used and *why* it was chosen. Use this for viva prep: for every row
you should be able to open the file, point at the line, and explain the
"why" column in one sentence.

Files: `utils.c/h`, `sensors.c/h`, `helmet.c/h`, `alert.c/h`,
`logger.c/h`, `main.c` (~1,100 lines total).

---

## 1. Types & data organization

| Concept | Where | How it is used | Why this choice |
|---|---|---|---|
| `struct` | `sensor_reading`, `ride_session`, `helmet_settings`, `helmet_decision`, `alert_msg`, `ride_record`, `timestamp` | Group related data into one value passed by pointer | One "sensor tick" or "ride" is a single thing; structs model it, keep function signatures short |
| `struct` containing `struct` | `alert_msg` has `timestamp at;` | Timestamp embedded in each alert | Shows composition, not just flat structs |
| `enum` | `helmet_verdict`, `sensor_event_kind`, `HelmetState` (Wokwi) | Finite set of outcomes: OK / NO HELMET / DRUNK / CRASH | Named constants >> magic numbers; `switch` on enums is exhaustive |
| `typedef` | all of the above | `typedef enum {...} helmet_verdict;` | Cleaner names without the `enum`/`struct` keyword everywhere |
| `const` | `ALCOHOL_LIMIT_G`, `PIN_GAS` etc., `const helmet_settings *cfg` parameters | Compile-time constants; read-only function params | Prevents accidental modification; documents intent ("this function does not change settings") |
| `static` | every internal helper (`pick_anomaly`, `trim_tail`, `make_decision`, `split_csv_line`, `is_int_in_range`…) | File-scope privacy | Internal linkage = no name clashes between modules; the C way of "private" |
| `volatile`-free design | — | No globals shared with ISRs in the C project | keeps the discussion honest: concurrency is not pretended here |

## 2. Functions & interfaces

| Concept | Where | How | Why |
|---|---|---|---|
| Modular headers + include guards | all `.h` files | `#ifndef SENSORS_H / #define / #endif` | Each module compiles alone; double-inclusion is impossible |
| Forward declaration | `utils.c` (`next_u64` before `ensure_rng`) | Prototype before use | Mutually recursive/static ordering without reordering logic |
| Function returning a `struct` | `helmet_check_interlock` → `helmet_decision` | Decision = allowed? + verdict + human reason | Data out, no globals; caller decides how to present it (console vs CSV vs SMS) |
| `varargs` (`va_list`) | `make_decision()` in helmet.c | `vsnprintf`-style formatted reason messages | One helper builds every decision message → no printf-duplication |
| Pass-by-pointer vs value | small structs by pointer (`const ride_record *`), single ints by value | Standard C practice | Avoid copying 200-byte records; `const` marks inputs |
| Function-per-responsibility | `read_alcohol_ppm`, `read_impact_g`, `pick_anomaly`, `split_csv_line`… | Small single-purpose functions | Testable (the 20 self-tests call them directly) |

## 3. Memory

| Concept | Where | How | Why |
|---|---|---|---|
| Dynamic memory `realloc` | `logger.c` `ride_list_push` | Grows the ride array geometrically (8 → 16 → …) | History size unknown at compile time; doubling gives amortized O(1) pushes |
| `malloc`-free ownership discipline | `ride_list_init` / `ride_list_free` pairs | Every load path frees; menu code uses a local `ride_list` | No leaks on any menu path — the #1 C bug avoided by construction |
| Stack arrays with explicit bounds | `char line[LINE_LEN]`, `char detail[SMS_LEN]` | Fixed buffers for parsing/formatting | Bounded, no heap churn for one-line work |
| `memset` init | `helmet_reset_ride`, `sensors_init_session`, `settings_default` | Zero a struct before use | Uninitialized struct fields are garbage in C — must be deliberate |
| String copy safety | `strncpy(dst, src, N-1); dst[N-1]='\0';` | Every name/phone/status copy | `strcpy` trusts the source; we never do |

## 4. Strings & parsing

| Concept | Where | How | Why |
|---|---|---|---|
| `strtol` / `strtod` strict parsing | `is_int_in_range`, `is_double_in_range` | Parse + require the *whole* line consumed + range check | `scanf("%d")` accepts `12abc` and poisons the buffer — classic viva trap, we replaced it |
| `strchr` | settings loader (`key=value`), CSV | Find the delimiter | Manual tokenizing where `strtok`'s global state would be wrong |
| `strcspn` | CSV loader | Strip `\r\n` regardless of order | Handles Windows/Unix line endings in one call |
| `snprintf` everywhere | All formatting | Bounded printf into fixed buffers | `sprintf` cannot overflow a buffer; `snprintf` cannot |
| `vsnprintf` | `make_decision` | varargs → formatted string, bounded | See varargs row above |
| Token splitting without `strtok` | `split_csv_line` | Walk the buffer, replace `,` with `\0`, collect pointers | In-place, no allocation, no global state; teaches pointer arithmetic honestly |
| Pointer arithmetic | same function | `fields[n++] = p + 1;` | The core C skill: pointers as cursor into a buffer |

## 5. Files & I/O

| Concept | Where | How | Why |
|---|---|---|---|
| `FILE*`, `fopen` modes | `"a"` for CSV/log appends, `"r"` for loads, `"w"` for settings save | Correct mode per job | Append never destroys history; `w` only where overwrite is intended |
| Line-oriented read `fgets` + check | every loader | `while (fgets(...))` with NULL check | Bounded line length; EOF handled by loop condition |
| Write + `fflush` + error check | `logger_append_ride`, `settings_save` | `ok = (fflush(fp)==0)` after writes | A crash mid-write should be detectable, not silent |
| Handle missing files gracefully | `settings_load`, `logger_load_all` | `fopen==NULL` → defaults / empty list | First run has no `data/` — must work anyway |
| Simple key=value format | `settings.txt` | One `key=value` per line | Human-editable, 20 lines of code, no library dependency |

## 6. Time & randomness

| Concept | Where | How | Why |
|---|---|---|---|
| `time.h` (`time`, `localtime`) | `now()` | Wall-clock timestamps for rides/alerts | Records need real timestamps |
| Custom `timestamp` struct + formatters | `format_timestamp`, `format_timestamp_compact` | `snprintf("%04d-%02d-…")` | One formatting rule everywhere; compact form for filenames |
| `clock()` for seed entropy | `ensure_rng` | Mixes time + address + clock | Same-second runs must not repeat the same "world" |
| **xorshift64\* PRNG** | `utils.c` `next_u64` | 64-bit xorshift with multiplier; high 53 bits for doubles | **Found bug:** MinGW `rand()` is a 15-bit truncated LCG whose consecutive outputs are correlated — one scripted session blocked 6 demo rides in a row. xorshift64* is fast, portable and statistically sound |
| Probability modelling | `chance(p)`, scheduled anomalies | `rand_double(0,1) < p` | Teaches event simulation without any sensor hardware |

## 7. Control flow & algorithms

| Concept | Where | How | Why |
|---|---|---|---|
| State machine | `helmet_verdict` decisions; Wokwi `enum HelmetState` | Each state has defined exits | Safety logic must be explicit; mirrors real interlock controllers |
| Scheduled simulation | `sensors_init_session` picks crash/drunk/helmet ticks once per ride | Flags persist (`drunk_injected`) | **Found bug:** per-tick probability rolls compound (0.2^n) — 83% of rides ended NO HELMET. Scheduling once makes demo odds honest |
| Aggregation | `logger_show_report` | Sums, maxima, per-status counts over the record array | Statistics without any library |
| Early-exit validation | `helmet_check_interlock` | helmet → alcohol → OK, return at first failure | Order encodes *policy priority*; short and provable |
| Latching + re-arm | Wokwi `lockSince` logic | BLOCKED re-arms when safe; CRASH latches until RST | **Found bug:** your report — lowering the gas slider never unlocked a blocked ride. Real interlocks latch; recovery must be deliberate |

## 8. Testing & tooling

| Concept | Where | How | Why |
|---|---|---|---|
| Self-test harness | `main.c` `run_self_tests`, `test_check` | 20 assertion-style checks printing PASS/FAIL, with a summary | No external framework needed; the logic is testable on any machine |
| Settings preservation in tests | round-trip test backs up & restores user file | Tests must not have side effects | **Found bug:** tests overwrote `settings.txt` with defaults |
| Warning-clean builds | `-std=c99 -Wall -Wextra -pedantic` | Fixed every warning (unused vars, implicit decls, nested functions) | Warnings are latent bugs; zero-warning is the professional baseline |
| Make + VS Code tasks | `Makefile`, `.vscode/tasks.json` | One-command build; F5 debug with gdb | Reproducible builds are part of the deliverable |
| Scenario-based verification | `RUN_PLAN.md` | Scripted menu input, `awk` CSV validation, live Serial capture | "It runs" is not evidence; recorded expected outputs are |

## 9. The five bugs found and fixed (your war stories)

1. **Windows `rand()` correlation** → 6 blocked rides in a row → replaced with xorshift64\* (`utils.c`)
2. **Compounding per-tick probabilities** → 83% of rides ended NO HELMET → schedule anomalies once per ride (`sensors.c`)
3. **Blocked rides logged 0 ppm** → real reading recorded before logging (`main.c`)
4. **Wrong SMS text on helmet block** → verdict-specific messages (`main.c` / `alert.c`)
5. **Self-tests clobbered user settings** → backup/restore around the round-trip (`main.c`)

Wokwi side added two more: **MQ-2 clean-air baseline** (~2.5 V idle → calibrate, report excess) and the **latch without re-arm** you reported.

## 10. One-breath answers (memorize these)

- "Why structs?" → "A sensor tick and a ride are single things; structs keep them that way and let decisions travel as data."
- "Why enums for verdicts?" → "Four outcomes, exhaustively switched — the compiler helps me, and the CSV gets a stable token."
- "Why `strtol` not `scanf`?" → "scanf accepts truncated junk like `12abc` and leaves the rest in the buffer; strtol lets me require the whole line and check range."
- "Why realloc doubling?" → "Amortized O(1) appends without knowing the history size up front; freed in one place, so no leaks."
- "Why xorshift64*?" → "I caught MinGW's 15-bit rand() producing correlated draws that skewed my simulation; xorshift is 64-bit, fast, and portable."
- "Why latch + re-arm?" → "A safety interlock must fail safe and recover only on evidence — same idea as the pre-AI safety gate concept in payment systems."
