/*
 * smart_door.ino
 * ──────────────
 * Module  : Smart Door (Obstruction Detection)
 * Sensor  : HC-SR04 Ultrasonic Sensor
 * Action  : If someone is hanging or blocking the door
 *           within DOOR_DISTANCE_CM, trigger buzzer AND
 *           send SMS alert to driver.
 *
 * Updates:
 *   v2 — Added watchdog timer (WDTO_8S)
 *
 * Wiring:
 *   Ultrasonic VCC  → 5V
 *   Ultrasonic GND  → GND
 *   Ultrasonic TRIG → Pin 3  (DOOR_TRIG_PIN)
 *   Ultrasonic ECHO → Pin 4  (DOOR_ECHO_PIN)
 *   Buzzer +        → Pin 9  (DOOR_BUZZER_PIN)
 *   GSM TX          → Pin 11 (DOOR_GSM_RX_PIN)
 *   GSM RX          → Pin 10 (DOOR_GSM_TX_PIN)
 */

#include <avr/wdt.h>
#include <SoftwareSerial.h>
#include "../shared/config.h"
#include "../shared/gsm_helper.h"

/* ── GSM Module Setup ─────────────────────────────────────────────────── */
SoftwareSerial gsm(DOOR_GSM_RX_PIN, DOOR_GSM_TX_PIN);

/* ── State Variables ──────────────────────────────────────────────────── */
bool          alert_sent    = false;
unsigned long last_sms_time = 0;  /* Timestamp of last SMS sent */
bool door_is_open = false;  /* Tracks if driver marked door as open */

/* ═══════════════════════════════════════════════════════════════════════ */
void setup(void)
{
    /* Disable watchdog first — prevents reboot loop after crash recovery */
    wdt_disable();

    pinMode(DOOR_TRIG_PIN,   OUTPUT);
    pinMode(DOOR_ECHO_PIN,   INPUT);
    pinMode(DOOR_BUZZER_PIN, OUTPUT);
    pinMode(DOOR_BUTTON_PIN, INPUT_PULLUP); /* Built-in pullup — no resistor needed */

    digitalWrite(DOOR_TRIG_PIN,   LOW);
    digitalWrite(DOOR_BUZZER_PIN, LOW);

    Serial.begin(9600);
    gsm.begin(9600);

    delay(2000);
    Serial.println("Smart Door: Ready");

    /* Enable watchdog AFTER setup is complete */
    wdt_enable(WDTO_8S);
}

/* ─────────────────────────────────────────────────────────────────────── */
/*  check_door_button()                                                    */
/*  Toggles door_is_open state each time driver presses the button.        */
/*  INPUT_PULLUP means button reads LOW when pressed, HIGH when released.  */
/* ─────────────────────────────────────────────────────────────────────── */
void check_door_button(void)
{
    static bool last_button_state = HIGH;  /* Previous button reading */
    bool current_state = digitalRead(DOOR_BUTTON_PIN);

    /* Detect press — transition from HIGH to LOW */
    if (last_button_state == HIGH && current_state == LOW)
    {
        door_is_open = !door_is_open;  /* Toggle state */

        if (door_is_open)
            Serial.println("Door marked OPEN by driver — alerts paused");
        else
            Serial.println("Door marked CLOSED by driver — alerts active");

        delay(50);  /* Debounce — ignore mechanical noise from button */
    }

    last_button_state = current_state;
}

/* ═══════════════════════════════════════════════════════════════════════ */
void loop(void)
{
    wdt_reset();

    check_door_button();  /* Always check button first */

    float distance_cm = get_distance_cm();

    Serial.print("Door distance: ");
    Serial.print(distance_cm);
    Serial.println(" cm");

    /*
     * Only check for obstruction if driver has marked door as CLOSED.
     * If door is open (at a bus stop), ignore distance readings entirely.
     */
    if (!door_is_open)
    {
        if (distance_cm <= DOOR_DISTANCE_CM)
        {
            digitalWrite(DOOR_BUZZER_PIN, HIGH);

            unsigned long now = millis();

            if (!alert_sent)
            {
                if (now - last_sms_time >= SMS_COOLDOWN_MS)
                {
                    trigger_door_alert();
                    last_sms_time = now;
                }
                alert_sent = true;
            }
        }
        else
        {
            digitalWrite(DOOR_BUZZER_PIN, LOW);
            alert_sent = false;
        }
    }
    else
    {
        /* Door is open — keep buzzer off and reset alert state */
        digitalWrite(DOOR_BUZZER_PIN, LOW);
        alert_sent = false;
    }

    delay(200);
}
/* ─────────────────────────────────────────────────────────────────────── */
float get_distance_cm(void)
{
    digitalWrite(DOOR_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(DOOR_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(DOOR_TRIG_PIN, LOW);

    long duration  = pulseIn(DOOR_ECHO_PIN, HIGH);
    float distance = (duration * 0.0343) / 2.0;

    return distance;
}

/* ─────────────────────────────────────────────────────────────────────── */
void trigger_door_alert(void)
{
    Serial.println("ALERT: Door obstruction detected! Sending SMS.");

    /*
     * Disable watchdog before SMS send.
     * AT commands take ~5 seconds — would falsely trigger reset.
     * Re-enable immediately after.
     */
    wdt_disable();
    gsm_send_sms(
        &gsm,
        DRIVER_PHONE_NUMBER,
        "ALERT: Bus door is blocked! Someone may be hanging. Check immediately."
    );
    wdt_enable(WDTO_8S);
}
