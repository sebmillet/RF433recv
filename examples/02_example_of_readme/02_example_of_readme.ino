#include "RF433recv.h"
#include <Arduino.h>

    // Depends on your schematic - here, the Radio Frequencies device is
    // plugged on Arduino' D2, that corresponds to interrupt number 0.
#define PIN_RFINPUT  2
#define INT_RFINPUT  0

RF_manager rf(PIN_RFINPUT, INT_RFINPUT);

void callback1(const BitVector *recorded) {
    Serial.print(F("Signal received from telecommand\n"));
}

void callback2(const BitVector *recorded) {
    Serial.print(F("Signal received from telecommand, code = 8A34E6BF\n"));
}

void setup() {
    pinMode(PIN_RFINPUT, INPUT);
    Serial.begin(115200);

        // [OTIO (no rolling code, 32-bit)]
    rf.register_Receiver(
    RFMOD_TRIBIT, // mod
     4956, // initseq
        0, // lo_prefix
        0, // hi_prefix
        0, // first_lo_ign
      580, // lo_short
     1274, // lo_long
        0, // hi_short (0 => take lo_short)
        0, // hi_long  (0 => take lo_long)
      520, // lo_last
     4956, // sep
       32, // nb_bits
    callback1,
     2000
    );
    rf.register_callback(callback2, 2000,
            new BitVector(32, 4, 0x8A, 0x34, 0xE6, 0xBF));

    Serial.print(F("Waiting for signal\n"));

    rf.activate_interrupts_handler();
}

void loop() {
    rf.do_events();
}
