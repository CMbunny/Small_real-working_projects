/*
 * smart_window.ino
 * ────────────────
 * Module  : Smart Window (Child Safety / Object Detection)
 * Sensor  : HC-SR04 Ultrasonic Sensor
 * Action  : If a child leans out or someone throws something
 *           within WINDOW_DISTANCE_CM, trigger buzzer alert.
 *
 * Updates:
 *   v2 — Added watchdog timer (WDTO_8S)
 *
 * Wiring:
 *   Ultrasonic VCC  → 5V
 *   Ultrasonic GND  → GND
 *   Ultrasonic TRIG → Pin 5  (WINDOW_TRIG_PIN)
 *   Ultrasonic ECHO → Pin 6  (WINDOW_ECHO_PIN)
 *   Buzzer +        → Pin 7  (WINDOW_BUZZER_PIN)
 */

#include <avr/wdt.h>
#include "../shared/config.h"

/* ── State Variables ──────────────────────────────────────────────────── */
bool alert_active = false;

/* ═══════════════════════════════════════════════════════════════════════ */
void setup(void)
{
    /* Disable watchdog first — prevents reboot loop after crash recovery */
    wdt_disable();

    pinMode(WINDOW_TRIG_PIN,   OUTPUT);
    pinMode(WINDOW_ECHO_PIN,   INPUT);
    pinMode(WINDOW_BUZZER_PIN, OUTPUT);

    digitalWrite(WINDOW_TRIG_PIN,   LOW);
    digitalWrite(WINDOW_BUZZER_PIN, LOW);

    Serial.begin(9600);
    Serial.println("Smart Window: Ready");

    /*
     * No GSM here so no delay(2000) boot wait needed.
     * Enable watchdog immediately after setup.
     */
    wdt_enable(WDTO_8S);
}

/* ═══════════════════════════════════════════════════════════════════════ */
void loop(void)
{
    /* Pat the watchdog — proves code is still running */
    wdt_reset();

    float distance_cm = get_distance_cm();

    Serial.print("Window distance: ");
    Serial.print(distance_cm);
    Serial.println(" cm");

    if (distance_cm <= WINDOW_DISTANCE_CM)
    {
        if (!alert_active)
        {
            Serial.println("ALERT: Object or child detected near window!");
            alert_active = true;
        }

        buzz_alert();
    }
    else
    {
        digitalWrite(WINDOW_BUZZER_PIN, LOW);
        alert_active = false;
    }

    delay(100);
}

/* ─────────────────────────────────────────────────────────────────────── */
float get_distance_cm(void)
{
    digitalWrite(WINDOW_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(WINDOW_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(WINDOW_TRIG_PIN, LOW);

    long duration  = pulseIn(WINDOW_ECHO_PIN, HIGH);
    float distance = (duration * 0.0343) / 2.0;

    return distance;
}

/* ─────────────────────────────────────────────────────────────────────── */
/*  buzz_alert()                                                           */
/*  Beeping pattern — grabs attention better than flat tone.               */
/*                                                                         */
/*  NOTE: wdt_reset() is called inside here because buzz_alert()           */
/*  uses delay(200) twice = 400ms per call. Over multiple beeps            */
/*  this can add up and risk a watchdog timeout.                           */
/* ─────────────────────────────────────────────────────────────────────── */
void buzz_alert(void)
{
    wdt_reset();                         /* Pat dog before delays */
    digitalWrite(WINDOW_BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(WINDOW_BUZZER_PIN, LOW);
    delay(200);
}
