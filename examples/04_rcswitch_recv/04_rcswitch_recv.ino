// 04_rcswitch_recv.ino

// Example sketch that comes along with RF433recv library.
// Decodes signals as specified in the RCSwitch library.

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

void callback_core(const BitVector *recorded, byte rcswitch_protocol) {
    Serial.print(F("Code received: "));
    if (recorded->get_nb_bytes() != 4) {
        char *printed_code = recorded->to_str();

        if (printed_code) {
            Serial.print(recorded->get_nb_bits());
            Serial.print(F(" bits: ["));
            Serial.print(printed_code);
            Serial.print(F("], RCSwitch protocol: "));
            Serial.print(rcswitch_protocol);
            Serial.print(F("\n"));

            free(printed_code);
        }
    } else {
        uint32_t v = (uint32_t)recorded->get_nth_byte(3) << 24 |
            (uint32_t)recorded->get_nth_byte(2) << 16 |
            (uint32_t)recorded->get_nth_byte(1) << 8 |
            (uint32_t)recorded->get_nth_byte(0);
        Serial.print(F("32 bits: "));
        Serial.print(v);
        Serial.print(F(", RCSwitch protocol: "));
        Serial.print(rcswitch_protocol);
        Serial.print(F("\n"));
    }

#ifdef DEBUG_EXEC_TIMES
    output_measureexectimes_stats();
#endif

}

#define BUILD_CALLBACK_FOR_RCSWITCH_PROTOCOL_N(n) \
    void callback##n(const BitVector *recorded) { \
        callback_core(recorded, n); \
    }

BUILD_CALLBACK_FOR_RCSWITCH_PROTOCOL_N(1);
BUILD_CALLBACK_FOR_RCSWITCH_PROTOCOL_N(2);
BUILD_CALLBACK_FOR_RCSWITCH_PROTOCOL_N(3);
BUILD_CALLBACK_FOR_RCSWITCH_PROTOCOL_N(4);
BUILD_CALLBACK_FOR_RCSWITCH_PROTOCOL_N(5);
BUILD_CALLBACK_FOR_RCSWITCH_PROTOCOL_N(6);
BUILD_CALLBACK_FOR_RCSWITCH_PROTOCOL_N(7);
BUILD_CALLBACK_FOR_RCSWITCH_PROTOCOL_N(8);
BUILD_CALLBACK_FOR_RCSWITCH_PROTOCOL_N(9);
BUILD_CALLBACK_FOR_RCSWITCH_PROTOCOL_N(10);
BUILD_CALLBACK_FOR_RCSWITCH_PROTOCOL_N(11);
BUILD_CALLBACK_FOR_RCSWITCH_PROTOCOL_N(12);

RF_manager rf(PIN_RFINPUT, INT_RFINPUT);

void setup() {
    pinMode(PIN_RFINPUT, INPUT);
    Serial.begin(115200);

// Protocols 7, 11 and 12 are difficult to activate along with others: the more
// decoders are setup, the longer the interrupt handler execution -> protocols
// with typical short durations become impossible to decode.

    rf.register_Receiver(RFMOD_TRIBIT, 10850, 0, 0, 0, 350, 1050, 0, 0, 350,
            10850, 32, callback1, 1000);
    rf.register_Receiver(RFMOD_TRIBIT, 6500, 0, 0, 0, 650, 1300, 0, 0, 650,
            6500, 32, callback2, 1000);
    rf.register_Receiver(RFMOD_TRIBIT, 7100, 0, 0, 0, 400, 900, 600, 1100, 400,
            7100, 32, callback3, 1000);
    rf.register_Receiver(RFMOD_TRIBIT, 2280, 0, 0, 0, 380, 1140, 0, 0, 380,
            2280, 32, callback4, 1000);
    rf.register_Receiver(RFMOD_TRIBIT, 7000, 0, 0, 0, 500, 1000, 0, 0, 500,
            7000, 32, callback5, 1000);
    rf.register_Receiver(RFMOD_TRIBIT_INVERTED, 10350, 0, 0, 450, 450, 900, 0,
            0, 0, 10350, 32, callback6, 1000);
//    rf.register_Receiver(RFMOD_TRIBIT, 9300, 0, 0, 0, 150, 900, 0, 0, 150, 9300,
//            32, callback7, 1000);
    rf.register_Receiver(RFMOD_TRIBIT, 26000, 0, 0, 0, 1400, 600, 3200, 3200,
            600, 26000, 32, callback8, 1000);
    rf.register_Receiver(RFMOD_TRIBIT_INVERTED, 26000, 0, 0, 1400, 1400, 600,
            3200, 3200, 0, 26000, 32, callback9, 1000);
    rf.register_Receiver(RFMOD_TRIBIT_INVERTED, 6570, 0, 0, 365, 1095, 365, 0,
            0, 0, 6570, 32, callback10, 1000);
//    rf.register_Receiver(RFMOD_TRIBIT_INVERTED, 9720, 0, 0, 270, 270, 540, 0, 0,
//            0, 9720, 32, callback11, 1000);
//    rf.register_Receiver(RFMOD_TRIBIT_INVERTED, 11520, 0, 0, 320, 320, 640, 0,
//            0, 0, 11520, 32, callback12, 1000);

    Serial.print(F("Waiting for signal\n"));

    rf.set_first_decoder_that_has_a_value_resets_others(true);
    rf.set_inactivate_interrupts_handler_when_a_value_has_been_received(true);
    rf.activate_interrupts_handler();
}

void loop() {
    // The author of this code often forgot to remove the SIMULATE_INTERRUPTS
    // macro (loosing a great deal of time to do useless real Radio Frequency
    // tests), the below check aims to avoid such mistakes!
#ifdef SIMULATE_INTERRUPTS
#error "Seriously?"
#endif
    rf.do_events();
}

// vim: ts=4:sw=4:tw=80:et
