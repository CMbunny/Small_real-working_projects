#ifndef CONFIG_H
#define CONFIG_H

/* ─────────────────────────────────────────
   SMART GLASSES - Pin Definitions
   ───────────────────────────────────────── */
#define GLASSES_IR_SENSOR_PIN   2   /* IR sensor output pin              */
#define GLASSES_BUZZER_PIN      8   /* Buzzer to wake the driver         */
#define GLASSES_GSM_TX_PIN      10  /* GSM module TX (connect to RX)     */
#define GLASSES_GSM_RX_PIN      11  /* GSM module RX (connect to TX)     */

/* ─────────────────────────────────────────
   SMART DOOR - Pin Definitions
   ───────────────────────────────────────── */
#define DOOR_TRIG_PIN           3   /* Ultrasonic trigger pin            */
#define DOOR_ECHO_PIN           4   /* Ultrasonic echo pin               */
#define DOOR_BUZZER_PIN         9   /* Buzzer for door alert             */
#define DOOR_BUTTON_PIN         2   /* Door open/close toggle button */
#define DOOR_GSM_TX_PIN         10
#define DOOR_GSM_RX_PIN         11

/* ─────────────────────────────────────────
   SMART WINDOW - Pin Definitions
   ───────────────────────────────────────── */
#define WINDOW_TRIG_PIN         5   /* Ultrasonic trigger pin            */
#define WINDOW_ECHO_PIN         6   /* Ultrasonic echo pin               */
#define WINDOW_BUZZER_PIN       7   /* Buzzer for window alert           */

/* ─────────────────────────────────────────
   THRESHOLDS
   ───────────────────────────────────────── */
#define BLINK_THRESHOLD_MS      500  /* Eye closed longer than this = drowsy */
#define SMS_COOLDOWN_MS       60000UL  /* Don't send SMS again for 60 seconds */
#define DROWSY_THRESHOLD_1      1   /* Buzzer only */
#define DROWSY_THRESHOLD_2      2   /* Buzzer + SMS */
#define DROWSY_THRESHOLD_3      3   /* Buzzer + SMS + continuous alarm */
#define DROWSY_RESET_MS    600000UL /* Reset counter after 10 minutes */
#define CALIBRATION_SAMPLES          50   /* Number of readings to take at startup */
#define DOOR_DISTANCE_CM        20   /* Closer than this = door blocked (cm) */
#define WINDOW_DISTANCE_CM      15   /* Closer than this = window danger (cm)*/

/* ─────────────────────────────────────────
   GSM CONFIG
   ───────────────────────────────────────── */
#define DRIVER_PHONE_NUMBER     "+91XXXXXXXXXX"  /* Replace with real number */

#endif
