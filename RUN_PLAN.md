# End-to-End Run Plan — Smart Helmet Safety System

Every step below was executed and verified on this machine (2026-09-30).
Follow it top to bottom and you will get exactly the outputs shown.

---

## Part 1 — Desktop C program

### 1.1 Build

Open a terminal in the `smart-helmet` folder:

```bash
# In VS Code: Ctrl+Shift+B   (uses .vscode/tasks.json)
# Or manually:
gcc -std=c99 -Wall -Wextra -pedantic -o build/smart_helmet.exe src/*.c
```

**Expected:** no output at all = success (warnings would print here).
Verified: `BUILD_EXIT:0`, zero warnings.

### 1.2 Run

```bash
./build/smart_helmet.exe        # or: .\build\smart_helmet.exe in PowerShell
```

**Expected banner:**
```
============================================================
              Smart Helmet Safety System v1.0
============================================================
  Alcohol interlock + crash detection demo (pure C).
  Data folder: data/
```
Then the 9-item main menu (0–8).

### 1.3 Recommended demo sequence (what to press, what you'll see)

| Step | Press | What happens | What proves it |
|---|---|---|---|
| 1 | `8` → Enter | Self-tests | `Result: 20/20 tests passed.` |
| 2 | `2` → Enter | Interlock check only | Random verdict: OK / NO HELMET / DRUNK with reason |
| 3 | `3` then `1` | Force crash ride | Ticks, then `<< Impact >>`, SMS box, `Status : CRASH` |
| 4 | `1` ×2 | Random rides | Mixture of COMPLETED/DRUNK/NO HELMET outcomes |
| 5 | `4` | Ride history | Table lists every ride from `data/rides.csv` |
| 6 | `5` | Safety report | Totals add up to the rides you just ran |
| 7 | `6` | SMS log | One line per alert sent |
| 8 | `7` → Enter through all | Settings screen | Shows limits; Enter keeps each value |
| 9 | `0` | Exit | `Ride safe! Goodbye.` |

**Expected data files after this run** (in `data/`):
- `rides.csv` — one row per ride, 11 columns, `CRASH` row has max_impact ≥ 4.0
- `alerts.log` — one line per SMS (crash + any blocks)
- `settings.txt` — your saved limits and contact

### 1.4 Scenario → status mapping (for viva)

| Sensor condition | Status in CSV | SMS? |
|---|---|---|
| Helmet on, alcohol < 60 ppm all ride | OK (COMPLETED) | no |
| Alcohol ≥ 60 ppm at pre-ride check | DRUNK (blocked, interlock_blocks=1) | yes — INTERLOCK |
| Helmet off any time | NO HELMET | yes when it blocks a start |
| Impact ≥ 4 g mid-ride | CRASH | yes — CRASH ALERT |

---

## Part 2 — Wokwi simulation (browser)

### 2.1 Open

Saved project: **https://wokwi.com/projects/476596023679560705**
(fallback: rebuild from `wokwi/` folder files per `wokwi/WOKWI_GUIDE.md` §1)

### 2.2 Run — full verified demo script (~2.5 min)

| Step | Your hands | Expected on screen | Verified? |
|---|---|---|---|
| 1 | Press ▶ Play | Serial: banner, `MPU6050 ready.`; LCD: `SMART HELMET` | ✓ |
| 2 | **Hold the blue strap button** (helmet worn) | After ≤2 s: `[PRE-RIDE] helmet=ON alcohol≈0.0` → `>> Ignition ON. Ride started.`; green LED on; 1 beep | ✓ |
| 3 | Keep holding; watch Serial | `Tick 1..7` with alcohol 0.0–0.3, impact ~0 g | ✓ |
| 4 | Keep holding until **tick 8** | `<< SIMULATED IMPACT (6 g) >>` → LCD `CRASH DETECTED!`; 5 beeps; red LED; **CRASH ALERT SMS** in Serial | ✓ |
| 5 | Note the lock | `(crash lock - press RST for a new ride)` repeating | ✓ |
| 6 | Press the Uno **RST button** | Back to boot → pre-ride loop | ✓ |
| 7 | Hold strap + **drag gas ppm slider to 5000+** | `[PRE-RIDE] alcohol≈2.5 (limit 1.00)` → LCD `ALCOHOL HIGH`; 3 beeps; **INTERLOCK SMS**; red LED | ✓ |
| 8 | **Don't restart.** Drag ppm back to ~400, keep holding strap | After ~5 s: `>> Sensors normal again - system re-armed.` → pre-ride resumes → ride starts | ✓ |
| 9 | Release the strap mid-ride | `HELMET REMOVED!` → INTERLOCK SMS → locked → re-arms when strap re-held | ✓ |

### 2.3 What to say while demoing (30-second narration)

> "The MQ-2 gas sensor reads alcohol as voltage above a calibrated clean-air
> baseline. The MPU6050 measures impact in g. Ignition only unlocks when the
> strap is closed and alcohol is under the limit. A crash above 4 g aborts the
> ride and sends an emergency SMS. A blocked ride re-arms itself once the
> rider is safe again — but a crash stays locked until reset, like real
> vehicle interlocks."

---

## Part 3 — Verification checklist (all ✓ verified 2026-09-30)

| Check | Result |
|---|---|
| Clean build `-std=c99 -Wall -Wextra -pedantic` | 0 errors, 0 warnings |
| Self-tests | 20/20 PASS |
| 3-ride scenario suite | statuses consistent, 0 malformed CSV rows |
| Crash row integrity | max_impact ≥ 4.0 g PASS |
| Settings round-trip preserved user file | PASS |
| Wokwi live project == repo `wokwi/sketch.ino` | identical (baseline + re-arm + real strap) |
| Wokwi full run: precheck → ride → crash → SMS → lock | verified live in Serial |
| Wokwi re-arm after block | verified live: `system re-armed` |
| GitHub | 3 commits pushed, working tree clean |

## Troubleshooting quick table

| Symptom | Fix |
|---|---|
| `gcc: command not found` | Use full path `C:\Users\vpshr\mingw64\bin\gcc.exe` (or VS Code task) |
| `Permission denied` writing exe | A debug session holds the exe — stop it (Shift+F5) or `taskkill /IM smart_helmet.exe /F` |
| Wokwi "Build Servers Busy" | Free-tier queue; press ▶ again, it caches previous compiles |
| Wokwi: stuck on IGNITION LOCKED | It's the safety latch — BLOCKED re-arms ~5 s after sensors are safe; CRASH needs RST |
| MPU6050 not found | SDA→A4, SCL→A5 swapped or missing |
