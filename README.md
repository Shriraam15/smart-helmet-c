# Smart Helmet Safety System — Pure C Project

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A menu-driven console application that simulates an intelligent two-wheeler helmet:
**alcohol detection with an ignition interlock, crash detection with emergency SMS
alerts, helmet-wear enforcement, and ride history logging with a safety report.**

## Screenshots

| | |
|---|---|
| ![Wokwi circuit](docs/wokwi-circuit.png) | ![Locked state](docs/wokwi-locked.png) |
| ![Crash alert](docs/wokwi-crash.png) | ![Console ride](docs/console-ride.png) |

*(In progress — see [docs/README.md](docs/README.md) for the capture checklist.)*

A menu-driven console application that simulates an intelligent two-wheeler helmet:
**alcohol detection with an ignition interlock, crash detection with emergency SMS
alerts, helmet-wear enforcement, and ride history logging with a safety report.**

Written entirely in standard C99 (no external libraries). Built for a second-year
C programming course; every module demonstrates core C concepts listed at the
bottom of this file.

## Features

| Feature | What it does |
|---|---|
| Alcohol detection | Simulated MQ-3 sensor reads ppm inside the helmet cavity every tick |
| Ignition interlock | Bike refuses to start unless the helmet is worn **and** alcohol is under the limit |
| Helmet-wear enforcement | Strap sensor; removing the helmet mid-ride ends the ride safely |
| Crash detection | Simulated accelerometer; impact ≥ threshold (default 4 g) = accident |
| Emergency SMS alerts | Crash/interlock alerts are "transmitted" to the emergency contact and saved to a log |
| Ride logging | Every ride is appended to a CSV file with timestamps and sensor maxima |
| Safety report | Statistics: status counts, averages, peaks, peak day, helmet removals |
| Settings | Legal alcohol limit, crash threshold, rider name, emergency contact — persisted to disk |
| Self-tests | 24 built-in checks of the interlock/crash logic (menu option 8) |

## Wokwi simulation (no hardware needed)

A browser-presentable Arduino Uno version of this project lives in [`wokwi/`](wokwi/):
same interlock and crash logic, running on real simulated parts (MQ-2 gas sensor,
MPU6050 accelerometer, I2C LCD, buzzer, LEDs, helmet-strap button). Open
[`wokwi/WOKWI_GUIDE.md`](wokwi/WOKWI_GUIDE.md) for setup, a 3-minute demo script,
and viva Q&A.

## Project structure

```
smart-helmet/
├── Makefile              (optional: needs make, see below)
├── README.md
├── wokwi/                (Arduino Uno browser simulation — see WOKWI_GUIDE.md)
├── STEP_BY_STEP_GUIDE.md (learn/rebuild/explain guide)
├── .vscode/              (build task, debugger, settings — just press Ctrl+Shift+B)
├── data/                 (created at runtime: rides.csv, alerts.log, settings.txt)
└── src/
    ├── main.c            menu loop, ride simulation, self-tests
    ├── sensors.c/.h      simulated MQ-3 + accelerometer + strap sensor
    ├── helmet.c/.h       interlock & crash decisions, settings, ride session
    ├── alert.c/.h        SMS compose/send/log/history
    ├── logger.c/.h       CSV append/parse, history table, statistics
    └── utils.c/.h        time, RNG, safe input, formatting
```

## How to run

### Easiest: VS Code (this machine is already set up)

1. Open the `smart-helmet` folder in VS Code (accept the C/C++ extension when prompted).
2. **Terminal → Run Build Task** (or `Ctrl+Shift+B`) — builds `build/smart_helmet.exe`.
3. **Run → Run Without Debugging** — or in the terminal: `.\build\smart_helmet.exe`.

### Command line (any machine with gcc)

```bash
cd smart-helmet
gcc -std=c99 -Wall -Wextra -pedantic -o build/smart_helmet.exe src/*.c   # Windows
gcc -std=c99 -Wall -Wextra -pedantic -o build/smart_helmet src/*.c      # Linux/Mac
./build/smart_helmet
```

The compiler used on this machine: MinGW-w64 GCC 16.2.0 at `C:\Users\vpshr\mingw64`.

### Make (if make is installed)

```bash
make && make run
```

## Menu

```
1. Start a simulated ride          5. View safety report & statistics
2. Run ignition interlock check    6. View sent SMS alerts
3. Simulate crash on next ride     7. Settings
4. View ride history               8. Run self-tests
0. Exit
```

## How the simulation works

Sensors are **auto-simulated** so demos need no manual input. Each ride runs a
series of ticks (8–24). Per tick the program generates:

- **alcohol ppm** — sober baseline ≈ 18 ppm ± noise; an anomaly may spike it to 150–400 ppm
- **impact g** — normal road vibration ≈ 1.0–1.6 g; a crash anomaly produces 6–9.5 g
- **helmet worn** — strap closed; an anomaly may open it (and it stays open)

Random anomalies (roughly): 45% of rides get a drunk spike, 20% a helmet removal,
15% a crash. Menus 1/3 let you force a crash; the pre-ride check can also catch a
drunk rider or missing helmet *before* the ignition unlocks.

## Data files (created in `data/`)

**rides.csv** — one line per ride:
```
ride_id,rider,start,end,max_alcohol_ppm,max_impact_g,helmet_removed,drunk_events,interlock_blocks,crash_detected,status
1,Rider,2026-09-30 11:25:00,2026-09-30 11:25:01,25.3,7.4,0,0,0,1,CRASH
```

**alerts.log** — one line per SMS sent:
```
2026-09-30 11:25:00 | to=+91-9876543210 | [CRASH ALERT] Rider: CRASH DETECTED! Impact 7.4g, rider Rider. Send help.
```

**settings.txt** — key=value pairs, editable from menu 7 or any text editor.

## Sample run (abridged)

```
---------------- MAIN MENU ----------------
Select option: 1
=======================================================
               LIVE RIDE SIMULATION v1.0
=======================================================
  Rider: Rider | Contact: +91-9876543210
  [PRE-RIDE CHECK] helmet=ON alcohol=29 ppm
  Interlock: Helmet worn, alcohol 29 ppm (limit 60).
  >> Ignition ON. Ride started.
  Tick  5 | alcohol   24.2 ppm | impact 1.3 g | helmet ON
  Tick  6 | alcohol   22.8 ppm | impact 7.4 g | helmet ON  << Impact >>
  +----------------------------------------+
  |           SMS ALERT TRANSMITTED        |
  +----------------------------------------+
  To      : +91-9876543210
  Message : [CRASH ALERT] Rider: CRASH DETECTED! Impact 7.4g ...
  >> EMERGENCY: crash detected. Ride aborted, help alerted.
  --- RIDE SUMMARY #1 ---
  Status      : CRASH
  Max alcohol : 25.3 ppm (limit 60)
  Max impact  : 7.4 g (threshold 4.0)
```

## C concepts demonstrated (for grading)

- **Modular design** — 6 modules with headers, include guards, clean interfaces
- **struct / enum / typedef** — `sensor_reading`, `ride_session`, `helmet_settings`, `helmet_verdict`, ...
- **Dynamic memory** — `realloc`-grown `ride_list` for CSV records
- **File I/O** — CSV append + parse, settings load/save, log file read/write
- **String handling** — `strncpy`, `strchr`, `strcspn`, `snprintf`, trimming, field splitting
- **varargs** — `vsnprintf`-based formatted decision messages
- **`time.h`** — timestamps for rides and alerts
- **Randomized simulation** — seeded `rand()` with probability modelling
- **Robust input** — strict `strtol`/`strtod` parsers replacing unsafe `scanf`
- **Self-testing** — 24 assertion-style checks of the safety logic
- **Zero warnings** at `-std=c99 -Wall -Wextra -pedantic`

## Hardware mapping (for your report/viva)

The simulation is a faithful stand-in for a real build; each module maps to hardware:

| Simulated module | Real hardware |
|---|---|
| `sensors.c` alcohol | MQ-3 alcohol gas sensor |
| `sensors.c` impact | MPU-6050 accelerometer (G-force magnitude) |
| `sensors.c` helmet strap | Magnetic reed switch on the strap |
| `helmet.c` interlock | Relay driving the ignition circuit |
| `alert.c` SMS | GSM module (SIM800L / SIM900) via AT commands |
| `logger.c` CSV | SD card module or EEPROM |

Same decision thresholds (ppm limit, g-force cutoff) apply unchanged.

## License

Released under the [MIT License](LICENSE).
