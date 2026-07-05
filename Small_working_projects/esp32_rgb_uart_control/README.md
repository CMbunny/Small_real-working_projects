# ESP32 RGB LED Control via UART

Control an RGB LED's color by sending binary commands over UART to an ESP32 (ESP-IDF, no Arduino core).

## What it does

The ESP32 listens on UART1. A connected device (PC, another microcontroller, etc.) sends a 4-byte binary frame, and the ESP32 sets the RGB LED to that color using hardware PWM (LEDC).

## Hardware

- ESP32 devkit
- Common-cathode RGB LED (or 3 separate LEDs) with current-limiting resistors
- USB-to-serial adapter (if driving UART1 from a PC)

| Signal        | GPIO |
|---------------|------|
| UART1 TX      | 17   |
| UART1 RX      | 16   |
| Red channel   | 4    |
| Green channel | 5    |
| Blue channel  | 18   |

## UART config

- Baud rate: 9600
- 8 data bits, no parity, 1 stop bit
- No flow control

## Protocol

4 bytes per command, binary (not ASCII):

```
0xFF  R  G  B
```

- `0xFF` — fixed marker byte, marks the start of a frame
- `R`, `G`, `B` — raw byte values 0–255 for each channel

Example: to set the LED to a dim purple (R=128, G=0, B=64), send:

```
FF 80 00 40
```

**Why a marker byte:** UART can drop or corrupt a byte. Without a marker, a single dropped byte permanently shifts every following frame (R becomes B, etc.) with no way to recover. The parser is a state machine that waits for `0xFF`, then reads exactly the next 3 bytes as R, G, B. A corrupted frame is discarded; the next `0xFF` resyncs it.

**Known limitation:** if sync is already lost, a data byte that happens to equal `0xFF` can be misread as a marker, costing one extra frame to resync. Not fixed in this version — a future version could use byte-stuffing/escaping to remove this edge case entirely.

## Error handling

- **Init failures** (UART or LEDC setup fails at boot) → logged via `ESP_LOGE`, then the device restarts (`esp_restart()`).
- **Runtime failures** (a single failed `ledc_set_duty` call while parsing a frame) → logged, but the device does *not* restart. Restarting on every runtime hiccup would make the LED unusable during normal operation.

## Project structure

```
esp32_rgb_uart_control/
├── CMakeLists.txt
├── README.md
└── main/
    ├── CMakeLists.txt
    ├── app_main.c        # calls init functions, starts the system
    ├── uart_handler.c/h  # UART setup + frame-parsing state machine
    └── led_control.c/h   # LEDC (PWM) setup + set_rgb()
```

## Build and flash

```bash
idf.py set-target esp32
idf.py build
idf.py -p <PORT> flash monitor
```

## Testing

Send a frame using a hex-capable serial tool or a Python script with `pyserial`:

```python
import serial
s = serial.Serial('<PORT>', 9600)
s.write(bytes([0xFF, 0x80, 0x40, 0x20]))  # marker, R, G, B
```

Confirm:
1. A valid 4-byte frame changes the LED color.
2. A partial/garbage frame (e.g. sending `80 40` with no marker) does **not** falsely light up the LED.
3. A valid frame sent right after a garbage one still works (confirms resync).
