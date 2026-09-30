# Wokwi Simulation Guide — Smart Helmet Safety System

Present the smart helmet in your **browser** — no hardware needed. The Wokwi
project mirrors the desktop C program: same interlock logic, same crash
detection, same SMS alerts.

> **Honesty note for your teacher:** Wokwi has no MQ-3 (alcohol-specific)
> part, so this uses the **MQ-2 gas sensor** — same analog principle
> (gas concentration → voltage), different target gas. On real hardware you
> would swap in the MQ-3; the code and thresholds are identical in shape.

---

## 1. Setup (2 minutes)

1. Go to **https://wokwi.com** and sign in (free Google account works).
2. Click **New Project → Arduino Uno**.
3. Open the **sketch.ino** tab, delete the starter code, and paste the
   contents of `wokwi/sketch.ino`.
4. Open the **diagram.json** tab, delete everything, and paste the contents
   of `wokwi/diagram.json`.
5. Click the **Library Manager** ("Add Library…" button in the sketch tab),
   search and add: **Adafruit MPU6050** (its dependencies Adafruit Unified
   Sensor / BusIO are added automatically) and **LiquidCrystal I2C**.
6. Press the green **▶ Play** button.

**Or the fast way:** create the project, then use **File → Import from
file** with these two files, or simply copy-paste both tabs.

---

## 2. What's on the board

| Part | Role (maps to your C project) | Connection |
|---|---|---|
| Arduino Uno | The brain (your whole `src/`) | — |
| LCD1602 (I2C, addr 0x27) | Status display | SDA→A4, SCL→A5, VCC→5V, GND→GND |
| MQ-2 gas sensor | `sensors.c` alcohol detection | AO→A0, VCC→5V, GND→GND |
| MPU6050 accelerometer | `sensors.c` crash detection | SDA→A4, SCL→A5 (same I2C bus) |
| Buzzer | `alert.c` alarm | +→D2, −→GND |
| Green LED + 220Ω | Ignition allowed | D3 → LED anode → cathode → GND |
| Red LED + 220Ω | Ignition locked | D4 → LED anode → cathode → GND |
| Blue pushbutton | Helmet strap sensor | one leg → D5, other → GND |

**Why I2C for the LCD?** Two wires instead of seven — that is exactly why
real products use it, and it leaves analog/digital pins free. Both the LCD
(0x27) and MPU6050 (0x68) share the same two wires because each has a
different I2C address. That sentence is a great viva answer.

---

## 3. How to run the demo (3-minute script)

| Minute | Do this | What it shows |
|---|---|---|
| 0:00 | Press ▶ Play | LCD boots "SMART HELMET", Serial shows system banner |
| 0:15 | Hover the **gas sensor** → drag ppm slider up | Pre-ride check: "ALCOHOL HIGH", red LED, buzzer, SMS in Serial |
| 0:45 | Drag ppm back down (~400) | Next check passes → "RIDE STARTED", green LED, 1 beep |
| 1:15 | Press and hold the **strap button** | This is "helmet worn"; riding ticks stream on Serial + LCD |
| 1:45 | **Release** the strap button | "HELMET REMOVED" → ride ends, red LED, INTERLOCK SMS |
| 2:15 | Press ▶ again (or RST), keep strap pressed | Ride starts again; at **tick 8 an automatic 6 g impact** fires |
| 2:30 | Watch the crash | "CRASH DETECTED", buzzer alarm, CRASH ALERT SMS in Serial |

**Alternative to the auto-crash:** while riding, drag the MPU6050's
acceleration sliders — any total acceleration over 4 g triggers the crash.

> The auto-demo is built in because Wokwi has no "shake the sensor" mouse
> action. In the sketch these are `AUTO_CRASH_TICK` and `AUTO_GAS_TICK` —
> set them to `0` to disable and test purely with the sliders.

---

## 4. Code walkthrough (be able to explain these)

**The state machine** — the heart of both versions:

```
STATE_PRECHECK ──(helmet + alcohol OK)──> STATE_RIDING
      │                                        │
      ├─ no helmet / alcohol ──> STATE_BLOCKED │
      │                             (locked)   │
      └────────────────────────────────────>── ├─ impact ≥ 4 g ──> STATE_CRASH
                                               ├─ helmet off ────> STATE_BLOCKED
                                               └─ alcohol high ──> STATE_BLOCKED
```

Key functions (compare them to your C code!):

| Arduino function | C project equivalent |
|---|---|
| `readAlcohol()` | `read_alcohol_ppm()` in sensors.c — analogRead replaces rand() |
| `readImpactG()` | `read_impact_g()` — sqrt(x²+y²+z²) − 1 g gravity |
| `helmetWorn()` | the strap flag — a real button replaces the simulated one |
| `showBlocked()` / `showCrash()` | `helmet_evaluate_tick()` verdicts + `alert_send()` |
| `sendSMS()` | `alert_send()` — Serial Monitor instead of a log file |

**Why `INPUT_PULLUP`?** The button leg is wired to GND; the pin is held
HIGH internally, and pressing the button pulls it LOW. No resistor needed.
`digitalRead(...) == LOW` therefore means "pressed = strap closed".

**Why subtract 1 g?** The MPU6050 always feels Earth's gravity (~1 g on Z
at rest). The helmet cares about *dynamic* acceleration — the jolt of a
crash — so the resting value is removed before comparing with the 4 g
threshold. Same idea as the C project's `IMPACT_IDLE_G`.

---

## 5. Likely viva questions & strong answers

**Q: Why is the alcohol check before ignition and also during the ride?**
A: A rider could start sober and drink during the ride, or trick a single
check. Continuous monitoring closes both gaps — same as the tick loop in
the C version.

**Q: Why 4 g as the crash threshold?**
A: Below ~3 g covers normal bumps and dropping the helmet; literature on
bike accidents puts impact spikes well above 4–6 g. It is a configurable
`const` — on the desktop version it is a setting in `data/settings.txt`.

**Q: Why is the MQ-2 reading analog, not digital?**
A: The DO pin only says "above/below a preset threshold"; the AO pin gives
the actual concentration so the firmware can apply its own limit, log
values, and show trends. (In the C project, ppm values do exactly this.)

**Q: What happens if the MPU6050 disconnects?**
A: `mpu.begin()` fails and the sketch halts with an error — a safety system
must fail loudly, not silently keep riding. (Fail-safe design.)

**Q: Why is the state machine better than nested if-else?**
A: Each state has one clear job; transitions are explicit; adding a new
state (e.g. POST-CRASH WAIT) touches one place. The C version expresses the
same structure with the `helmet_verdict` enum.

**Q: How would you send a real SMS?**
A: Replace `sendSMS()` with AT commands to a SIM800L GSM module on
SoftwareSerial: `AT+CMGF=1` (text mode), `AT+CMGS="+91..."`, message,
Ctrl+Z. The logic around it does not change.

**Q: Where is the "memory" of rides here?**
A: The desktop C project keeps rides in `rides.csv` (file I/O). On Arduino
you would log to an SD card module over SPI — the `logger.c` module maps
one-to-one.

---

## 6. Troubleshooting

| Symptom | Fix |
|---|---|
| LCD shows nothing | Check SDA/SCL not swapped; verify `pins: "i2c"` in diagram.json |
| "MPU6050 not found" | SDA→A4 and SCL→A5 must be exact on the Uno |
| Gas always triggers | Drag the ppm slider back to ~400 before the pre-ride check |
| No sound | Click the buzzer part; volume attr is 0.6 — raise to 1.0 |
| Library errors | Library Manager: add "Adafruit MPU6050" and "LiquidCrystal I2C" |
