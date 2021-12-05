// 01_recv.ino

// Example sketch that comes along with RF433recv library
// Simply receives codes

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

#define ASSERT_OUTPUT_TO_SERIAL

#define assert(cond) { \
    if (!(cond)) { \
        recv_ino_assert_failed(__LINE__); \
    } \
}

static void recv_ino_assert_failed(int line) {
#ifdef ASSERT_OUTPUT_TO_SERIAL
    Serial.print(F("\n01_recv.ino:"));
    Serial.print(line);
    Serial.println(F(": assertion failed, aborted."));
#endif
    while (1)
        ;
}

void callback(const BitVector *recorded) {
    Serial.print(F("Code received: "));
    char *printed_code = recorded->to_str();
    if (printed_code) {
        Serial.print(recorded->get_nb_bits());
        Serial.print(F(" bits: ["));
        Serial.print(printed_code);
        Serial.print(F("]\n"));
    }
    if (printed_code)
        free(printed_code);
}

RF_manager rf(PIN_RFINPUT, INT_RFINPUT);

void setup() {
    pinMode(PIN_RFINPUT, INPUT);
    Serial.begin(115200);

        // FIRST CODE, inspired from FLO
    rf.register_Receiver(
        RFMOD_TRIBIT_INVERTED, // mod
        23936, // initseq
            0, // lo_prefix
            0, // hi_prefix
          684, // first_lo_ign
          684, // lo_short
         1360, // lo_long
            0, // hi_short (0 => take lo_short)
            0, // hi_long  (0 => take lo_long)
          676, // lo_last
        23928, // sep
           12, // nb_bits
     callback, // Callback when code received
             0
    );

        // SECOND CODE, inspired from OTIO (but shorter)
    rf.register_Receiver(
        RFMOD_TRIBIT, // mod
         6976, // initseq
            0, // lo_prefix
            0, // hi_prefix
            0, // first_lo_ign
          562, // lo_short
         1258, // lo_long
            0, // hi_short (0 => take lo_short)
            0, // hi_long  (0 => take lo_long)
          528, // lo_last
         6996, // sep
           16, // nb_bits
     callback, // Callback when code received
             0
    );

        // ADF (no rolling code, 32-bit)
//    rf.register_Receiver(
//        RFMOD_MANCHESTER, // mod
//         5500, // initseq
//            0, // lo_prefix
//            0, // hi_prefix
//            0, // first_lo_ign
//         1166, // lo_short
//            0, // lo_long
//            0, // hi_short (0 => take lo_short)
//            0, // hi_long  (0 => take lo_long)
//            0, // lo_last
//         5500, // sep
//           32, // nb_bits
//           callback,
//           2000
//    );
//    rf.register_callback(callback_man_1, 2000,
//            new BitVector(32, 4, 0x4A, 0x9B, 0x9C, 0x9D));
//    rf.register_callback(callback_man_2, 2000,
//            new BitVector(32, 4, 0x4E, 0x9F, 0xA0, 0xA1));

        // FLO/R (rolling code, 72-bit, has a prefix)
//    rf.register_Receiver(
//        RFMOD_TRIBIT, // mod
//        18000, // initseq
//         1450, // lo_prefix
//         1450, // hi_prefix
//            0, // first_lo_ign
//          450, // lo_short
//          900, // lo_long
//            0, // hi_short (0 => take lo_short)
//            0, // hi_long  (0 => take lo_long)
//         1400, // lo_last
//        18000, // sep
//           72, // nb_bits
//        callback,
//         2000
//    );


    rf.set_opt_wait_free_433(false);
    rf.activate_interrupts_handler();

    assert(true); // Written to avoid a warning "unused function"
                  // FIXME (remove assert management code? Leaving it for now.)
}

extern const size_t timings_len;
extern byte timings_index;

void handle_int_receive();
bool has_read_all_timings();

void loop() {

    delay(1500);
    Serial.print(F("----- BEGIN TEST -----\n"));
    while (!has_read_all_timings()) {
        handle_int_receive();
        rf.do_events();
    }
    Serial.print(F("----- END TEST -----\n"));

    while (1)
        ;

}

// vim: ts=4:sw=4:tw=80:et
