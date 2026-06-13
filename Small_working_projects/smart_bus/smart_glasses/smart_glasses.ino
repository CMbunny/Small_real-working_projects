/*
 * smart_glasses.ino
 * ─────────────────
 * Module  : Smart Glasses (Driver Drowsiness Detection)
 * Sensor  : IR Sensor
 * Action  : If eyes stay closed beyond BLINK_THRESHOLD_MS,
 *           trigger buzzer AND send SMS to driver's phone.
 *
 * Updates:
 *   v2 — Added watchdog timer (WDTO_8S)
 *
 * Wiring:
 *   IR Sensor VCC  → 5V
 *   IR Sensor GND  → GND
 *   IR Sensor OUT  → Pin 2  (GLASSES_IR_SENSOR_PIN)
 *   Buzzer +       → Pin 8  (GLASSES_BUZZER_PIN)
 *   GSM TX         → Pin 11 (GLASSES_GSM_RX_PIN)
 *   GSM RX         → Pin 10 (GLASSES_GSM_TX_PIN)
 */

#include <avr/wdt.h>          /* Watchdog timer library — built into Arduino */
#include <SoftwareSerial.h>
#include "../shared/config.h"
#include "../shared/gsm_helper.h"

/* ── GSM Module Setup ─────────────────────────────────────────────────── */
SoftwareSerial gsm(GLASSES_GSM_RX_PIN, GLASSES_GSM_TX_PIN);

/* ── State Variables ──────────────────────────────────────────────────── */
unsigned long eye_closed_start = 0;
bool          eye_was_closed   = false;
bool          alert_sent        = false;
unsigned long last_sms_time     = 0;  /* Timestamp of last SMS sent */
int sensor_baseline = 0;  /* Calibrated baseline reading at startup */
int           drowsy_count      = 0;  /* How many drowsy events so far     */
unsigned long first_drowsy_time = 0;  /* Timestamp of first drowsy event   */

/* ═══════════════════════════════════════════════════════════════════════ */
void setup(void)
{
    /*
     * IMPORTANT: Disable watchdog first thing in setup.
     * Why: If Arduino rebooted BECAUSE of a watchdog timeout,
     * the watchdog stays active after reboot with a very short
     * timeout. Disabling it first prevents a reboot loop.
     */
    wdt_disable();

    pinMode(GLASSES_IR_SENSOR_PIN, INPUT);
    pinMode(GLASSES_BUZZER_PIN,    OUTPUT);
    digitalWrite(GLASSES_BUZZER_PIN, LOW);

    Serial.begin(9600);
    gsm.begin(9600);

    delay(2000);  /* Give GSM module time to boot */
    Serial.println("Smart Glasses: Ready");
     
    sensor_baseline = calibrate_sensor();
    /* Enable watchdog AFTER setup is complete.
     * WDTO_8S = reset if code freezes for 8 seconds.
     * Set it here, not before delay(2000), otherwise
     * the 2 second boot delay could trigger a reset.
     */
    wdt_enable(WDTO_8S);
}

/* ─────────────────────────────────────────────────────────────────────── */
/*  calibrate_sensor()                                                     */
/*  Takes 50 readings at startup with eyes OPEN and averages them.         */
/*  This becomes the baseline for "eye open" in current light conditions.  */
/* ─────────────────────────────────────────────────────────────────────── */
int calibrate_sensor(void)
{
    int total = 0;

    Serial.println("Calibrating IR sensor — keep eyes open...");

    for (int i = 0; i < CALIBRATION_SAMPLES; i++)
    {
        total += digitalRead(GLASSES_IR_SENSOR_PIN);
        delay(20);
    }

    int baseline = total / CALIBRATION_SAMPLES;

    Serial.print("Calibration done. Baseline: ");
    Serial.println(baseline);

    return baseline;
}

/* ─────────────────────────────────────────────────────────────────────── */
/*  handle_drowsiness_level()                                              */
/*  Escalates response based on how many times driver has been drowsy      */
/*  within the last 10 minutes.                                            */
/* ─────────────────────────────────────────────────────────────────────── */
void handle_drowsiness_level(void)
{
    unsigned long now = millis();

    /* Reset counter if 10 minutes have passed since first drowsy event */
    if (drowsy_count > 0 && (now - first_drowsy_time >= DROWSY_RESET_MS))
    {
        Serial.println("Drowsy counter reset — 10 minutes passed");
        drowsy_count      = 0;
        first_drowsy_time = 0;
    }

    /* Record timestamp of first event in this window */
    if (drowsy_count == 0)
        first_drowsy_time = now;

    drowsy_count++;

    Serial.print("Drowsy event count: ");
    Serial.println(drowsy_count);

    if (drowsy_count == DROWSY_THRESHOLD_1)
    {
        /* First time — buzzer only, no SMS */
        Serial.println("Level 1: Buzzer only");
        digitalWrite(GLASSES_BUZZER_PIN, HIGH);
    }
    else if (drowsy_count == DROWSY_THRESHOLD_2)
    {
        /* Second time — buzzer + SMS */
        Serial.println("Level 2: Buzzer + SMS");
        digitalWrite(GLASSES_BUZZER_PIN, HIGH);

        wdt_disable();
        gsm_send_sms(
            &gsm,
            DRIVER_PHONE_NUMBER,
            "WARNING: Driver drowsy twice in 10 minutes. Please take a break."
        );
        wdt_enable(WDTO_8S);
        last_sms_time = millis();
    }
    else if (drowsy_count >= DROWSY_THRESHOLD_3)
    {
        /* Third time or more — continuous alarm + urgent SMS */
        Serial.println("Level 3: Continuous alarm + urgent SMS");

        unsigned long now2 = millis();
        if (now2 - last_sms_time >= SMS_COOLDOWN_MS)
        {
            wdt_disable();
            gsm_send_sms(
                &gsm,
                DRIVER_PHONE_NUMBER,
                "URGENT: Driver critically drowsy! Pull over immediately."
            );
            wdt_enable(WDTO_8S);
            last_sms_time = millis();
        }

        /* Continuous beeping pattern for level 3 */
        for (int i = 0; i < 5; i++)
        {
            wdt_reset();
            digitalWrite(GLASSES_BUZZER_PIN, HIGH);
            delay(100);
            digitalWrite(GLASSES_BUZZER_PIN, LOW);
            delay(100);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════ */
void loop(void)
{
    /*
     * Pat the watchdog at the TOP of every loop.
     * This tells it: "I'm alive, reset the countdown."
     * If this line stops being reached, Arduino reboots in 8s.
     */
    wdt_reset();

    int  sensor_value  = digitalRead(GLASSES_IR_SENSOR_PIN);
    /*
    * If baseline is 0, light conditions favor HIGH = closed detection.
    * If baseline is 1, light conditions are inverted — flip the logic.
    */
    bool eye_is_closed = (sensor_baseline == 0)
                        ? (sensor_value == HIGH)
                        : (sensor_value == LOW);

    if (eye_is_closed)
    {
        if (!eye_was_closed)
        {
            eye_closed_start = millis();
            eye_was_closed   = true;
            alert_sent       = false;
            Serial.println("Eye closed — timer started");
        }
        else
        {
            unsigned long closed_duration = millis() - eye_closed_start;

            unsigned long now = millis();

            if (closed_duration >= BLINK_THRESHOLD_MS && !alert_sent)
            {
                handle_drowsiness_level();
                alert_sent = true;
            }
        }
    }
    else
    {
        if (eye_was_closed)
        {
            Serial.println("Eye opened — timer reset");
        }

        eye_was_closed = false;
        alert_sent     = false;
        digitalWrite(GLASSES_BUZZER_PIN, LOW);
    }

    delay(50);
}

/* ─────────────────────────────────────────────────────────────────────── */
void trigger_drowsiness_alert(void)
{
    Serial.println("ALERT: Driver drowsy! Triggering alarm and SMS.");

    digitalWrite(GLASSES_BUZZER_PIN, HIGH);

    /*
     * IMPORTANT: Disable watchdog before gsm_send_sms().
     * Why: Sending an SMS takes ~5 seconds due to AT command delays.
     * That's longer than our 8s timeout — risky on slower networks.
     * We disable it, send the SMS, then re-enable immediately after.
     */
    wdt_disable();
    gsm_send_sms(
        &gsm,
        DRIVER_PHONE_NUMBER,
        "ALERT: Driver drowsiness detected! Please pull over safely."
    );
    wdt_enable(WDTO_8S);
}
