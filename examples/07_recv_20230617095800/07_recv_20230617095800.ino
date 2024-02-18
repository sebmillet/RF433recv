#include "RF433recv.h"
#include <Arduino.h>

#define PIN_RFINPUT 2

#define ASSERT_OUTPUT_TO_SERIAL

void callback_generic(const BitVector *recorded)
{
    Serial.print(F("Code received: "));
    char *printed_code = recorded->to_str();

    if (printed_code)
    {
        Serial.print(recorded->get_nb_bits());
        Serial.print(F(" bits: ["));
        Serial.print(printed_code);
        Serial.print(F("]\n"));

        free(printed_code);
    }
}

void callback1(const BitVector *recorded)
{
    Serial.print(F("111Waiting for signal\n"));
    Serial.print(F("1> "));
    callback_generic(recorded);
}

RF_manager rf(PIN_RFINPUT);

void setup()
{
    pinMode(PIN_RFINPUT, INPUT);
    Serial.begin(115200);
    rf.register_Receiver(
        RFMOD_TRIBIT, // mod
        12000,        // initseq
        0,            // lo_prefix
        0,            // hi_prefix
        0,            // first_lo_ign
        431,          // lo_short
        1236,         // lo_long
        0,            // hi_short (0 => take lo_short)
        0,            // hi_long  (0 => take lo_long)
        430,          // lo_last
        12091,        // sep
        24,            // nb_bits
        callback1,
        0
    );

    rf.set_opt_wait_free_433(false);
    rf.activate_interrupts_handler();
}

void loop()
{
static unsigned long last_t = 0;

    rf.do_events();
    unsigned long t = millis();
    if (t - last_t >= 1000) {
        Serial.print(F("tic\n"));
        last_t = t;
    }
}
