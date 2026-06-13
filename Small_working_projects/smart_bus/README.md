# Smart Bus Safety System

A collection of Arduino-based safety modules built as a final year project.
Each module is independent and can be deployed separately on the bus.

---

## Modules

| Module | Sensor | Trigger | Action |
|---|---|---|---|
| Smart Glasses | IR Sensor | Eye closed > 500ms | Buzzer + SMS to driver |
| Smart Door | Ultrasonic (HC-SR04) | Object within 20cm | Buzzer + SMS to driver |
| Smart Window | Ultrasonic (HC-SR04) | Object within 15cm | Beeping buzzer |

---

## Folder Structure

small_real-world_projects/

└── smart-bus/

├── shared/

│   ├── config.h          ← All pin numbers and thresholds

│   └── gsm_helper.h      ← Reusable SMS sending function

├── smart_glasses/

│   └── smart_glasses.ino

├── smart_door/

│   └── smart_door.ino

└── smart_window/

└── smart_window.ino

---

## Hardware Required

### Smart Glasses
- Arduino Uno
- IR Sensor module
- SIM800L GSM Module
- Buzzer
- SIM card with active balance

### Smart Door
- Arduino Uno
- HC-SR04 Ultrasonic Sensor
- SIM800L GSM Module
- Buzzer
- Physical push button (door open/close toggle)
- SIM card with active balance

### Smart Window
- Arduino Uno
- HC-SR04 Ultrasonic Sensor
- Buzzer

---

## Pin Configuration

All pins defined in `shared/config.h`.
Change them there if your wiring is different.

| Module | Pin | Purpose |
|---|---|---|
| Smart Glasses | 2 | IR Sensor OUT |
| Smart Glasses | 8 | Buzzer |
| Smart Glasses | 10, 11 | GSM TX, RX |
| Smart Door | 2 | Door open/close toggle button |
| Smart Door | 3, 4 | Ultrasonic TRIG, ECHO |
| Smart Door | 9 | Buzzer |
| Smart Door | 10, 11 | GSM TX, RX |
| Smart Window | 5, 6 | Ultrasonic TRIG, ECHO |
| Smart Window | 7 | Buzzer |

---

## Feature Summary

### Smart Glasses — Drowsiness Escalation
| Event | Response |
|---|---|
| 1st drowsy in 10 min | Buzzer only |
| 2nd drowsy in 10 min | Buzzer + SMS warning |
| 3rd+ drowsy in 10 min | Continuous alarm + urgent SMS |
| After 10 minutes | Counter resets to zero |

### Smart Door — Button State Tracking
- Driver presses button to mark door **open** at bus stops — alerts paused
- Driver presses again to mark door **closed** — alerts active
- Prevents false alarms at every bus stop

### Smart Window — Beeping Alert
- Beeping pattern instead of flat tone for better attention grabbing
- No GSM — buzzer alert is sufficient for in-bus passengers

---

## Thresholds (Adjustable)

All in `shared/config.h`:

| Constant | Default | Meaning |
|---|---|---|
| `BLINK_THRESHOLD_MS` | 500ms | Eye closed longer than this = drowsy |
| `DOOR_DISTANCE_CM` | 20cm | Object closer than this = door blocked |
| `WINDOW_DISTANCE_CM` | 15cm | Object closer than this = window danger |
| `SMS_COOLDOWN_MS` | 60000ms | Minimum gap between SMS alerts |
| `DROWSY_RESET_MS` | 600000ms | Drowsiness counter resets after this |
| `CALIBRATION_SAMPLES` | 50 | IR sensor readings taken at startup |

---

## Setup

1. Open each `.ino` file in Arduino IDE
2. Update `DRIVER_PHONE_NUMBER` in `shared/config.h`
3. Upload each module to its own Arduino Uno
4. Wire components as shown in wiring comments inside each `.ino` file
5. On Smart Glasses startup — keep eyes open during calibration (1 second)

---

## Implemented Updates

- [x] Watchdog timer on all three modules (auto reboot on freeze)
- [x] SMS cooldown timer — no SMS spam (60 second gap)
- [x] IR sensor calibration at startup (Smart Glasses)
- [x] Door open/close state tracking via button (Smart Door)
- [x] Drowsiness escalation counter (Smart Glasses)

## Reserved for Later

- [ ] Power loss detection — SMS fired before shutdown (all modules)

---

## Tech Stack
- Language: C (Arduino flavor)
- Hardware: Arduino Uno
- Sensors: IR Sensor, HC-SR04 Ultrasonic
- Communication: SIM800L GSM Module (AT commands)