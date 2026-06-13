#ifndef GSM_HELPER_H
#define GSM_HELPER_H

#include <SoftwareSerial.h>

/*
 * gsm_send_sms()
 *
 * Sends an SMS using AT commands over SoftwareSerial.
 *
 * Parameters:
 *   gsm     - pointer to the SoftwareSerial object connected to GSM module
 *   number  - phone number string e.g. "+91XXXXXXXXXX"
 *   message - the SMS text to send
 */
void gsm_send_sms(SoftwareSerial *gsm, const char *number, const char *message)
{
    gsm->println("AT");           /* Check module is alive       */
    delay(1000);

    gsm->println("AT+CMGF=1");   /* Set SMS to text mode        */
    delay(1000);

    gsm->print("AT+CMGS=\"");    /* Start SMS send command      */
    gsm->print(number);
    gsm->println("\"");
    delay(1000);

    gsm->print(message);         /* Write message body          */
    delay(500);

    gsm->write(26);              /* Send Ctrl+Z to finish SMS   */
    delay(3000);                 /* Wait for send confirmation  */
}

#endif /* GSM_HELPER_H */