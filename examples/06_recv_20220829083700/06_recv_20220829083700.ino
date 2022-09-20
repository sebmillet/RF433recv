// 06_recv_20220829083700.ino

// Example sketch that comes along with RF433recv library.
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
    // Specifying the interrupt number is optional, you can leave it to the
    // constructor to work it out.
#define INT_RFINPUT  0

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
    // Second parameter is optional. Could also be:
//RF_manager rf(PIN_RFINPUT);

void setup() {
    pinMode(PIN_RFINPUT, INPUT);
    Serial.begin(115200);

        // Replace the below code with your telecommand timings.
        // See RF433any (https://github.com/sebmillet/RF433any) as to how to
        // work out these timings. In particular, you can use example:
        //   examples/01_main/01_main.ino
        // to output exactly what is needed to call register_Receiver().
        // This example complete URL is:
        //   https://github.com/sebmillet/RF433any/blob/main/examples/01_main/01_main.ino

        // [WRITE THE DEVICE NAME HERE]
    rf.register_Receiver(
        RFMOD_TRIBIT_INVERTED, // mod
        8550,             // initseq
        0,                // lo_prefix
        0,                // hi_prefix
        500,              // first_lo_ign
        500,              // lo_short
        500,              // lo_long
        1900,             // hi_short
        3800,             // hi_long
        0,                // lo_last
        8550,             // sep
        37,               // nb_bits
        callback_anycode, // callback (optional ; just to show received code)
        1000              // delay between two calls to callback
    );

    Serial.print(F("Waiting for signal\n"));

    rf.activate_interrupts_handler();
}

void loop() {
#ifdef SIMULATE_INTERRUPTS
#error "Seriously?"
#endif
    rf.do_events();
}

// vim: ts=4:sw=4:tw=80:et
