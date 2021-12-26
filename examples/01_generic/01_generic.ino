// 01_generic.ino

// Example sketch that comes along with RF433recv library
// Implements what is needed to respond to a 2-button telecommand: implement
// receiver and callback functions.
// The receiver timings and receiver codes can be worked out using RF433any
// library.

/*
  Copyright 2021 Sébastien Millet

  `RF433recv' is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  `RF433recv' is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program. If not, see
  <https://www.gnu.org/licenses>.
*/

//
// Schematic: Radio Frequencies RECEIVER plugged on D2
//

#include "RF433recv.h"
#include <Arduino.h>

#define PIN_RFINPUT  2
#define INT_RFINPUT  0

#define ARRAYSZ(a) (sizeof(a) / sizeof(*a))

void callback_anycode(const BitVector *recorded) {
    Serial.print(F("Code received: "));
    char *printed_code = recorded->to_str();

    if (printed_code) {
        Serial.print(recorded->get_nb_bits());
        Serial.print(F(" bits: ["));
        Serial.print(printed_code);
        Serial.print(F("]\n"));

        free(printed_code);
    }
}

void callback_button_up(const BitVector *recorded) {
    Serial.print(F("Button 'up' pressed\n"));
}

void callback_button_down(const BitVector *recorded) {
    Serial.print(F("Button 'down' pressed\n"));
}

RF_manager rf(PIN_RFINPUT, INT_RFINPUT);

void setup() {
    pinMode(PIN_RFINPUT, INPUT);
    Serial.begin(115200);

        // Replace the below code with your telecommand timings.
        // See RF433any (https://github.com/sebmillet/RF433any) as to how to
        // work out these timings. In particular, you can use example:
        //   ...
        // to output exactly what is needed to call register_Receiver().

        // [WRITE THE DEVICE NAME HERE] Yes sure, will do shortly.
    rf.register_Receiver(
        RFMOD_MANCHESTER, // mod
        11232,            // initseq
        0,                // lo_prefix
        0,                // hi_prefix
        0,                // first_lo_ign
        1170,             // lo_short
        2338,             // lo_long
        0,                // hi_short (0 => take lo_short)
        0,                // hi_long  (0 => take lo_long)
        1148,             // lo_last
        11232,            // sep
        32,               // nb_bits
        callback_anycode, // callback (optional ; just to show received code)
        0                 // delay between two calls to callback
    );
    rf.register_callback(callback_button_up, 1000,
            new BitVector(32, 4, 0xF0, 0x55, 0xAA, 0x00));
    rf.register_callback(callback_button_down, 1000,
            new BitVector(32, 4, 0xF0, 0x55, 0xAA, 0x01));

    Serial.print(F("Waiting for signal\n"));

    rf.activate_interrupts_handler();
}

void loop() {
    rf.do_events();
}

// vim: ts=4:sw=4:tw=80:et
