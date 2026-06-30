# nvs_test

Minimal standalone ESP-IDF project to verify NVS (Non-Volatile Storage) blob
write/read works correctly on the target chip.

## What it does

1. Initializes NVS flash (`nvs_flash_init`), erasing and reinitializing if the
   partition is corrupt or from an incompatible IDF version.
2. Opens namespace `test_ns`, writes a `test_blob_t` struct (`name="hello"`,
   `value=42`) under key `blob_key`, commits, closes.
3. Reopens read-only, reads the blob back into a fresh struct.
4. `memcmp`s write vs read data and logs PASS/FAIL.

## Requirements

- ESP-IDF v5.5.3 (assumed from your existing `idf.currentSetup` config —
  verify if you're on a different version)
- A connected ESP32-family board

## Build & flash

```sh
cd nvs_test
idf.py set-target esp32      # swap for your actual chip, e.g. esp32s3
idf.py -p <PORT> build flash monitor
```

## How to test persistence

- First flash: should log `Wrote:` then `Read:` with matching values, then `PASS`.
- Power-cycle (or reset) the board *without* reflashing: data should still be
  there if you check the monitor log — NVS isn't erased on every boot, only
  on version mismatch/corruption.
- To force a clean state: `idf.py erase-flash` before reflashing.

## File layout

```
nvs_test/
├── CMakeLists.txt
├── README.md
└── main/
    ├── CMakeLists.txt
    └── main.c
```
