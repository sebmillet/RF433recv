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

void callback_generic(const BitVector *recorded) {
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

void callback1(const BitVector *recorded) {
    Serial.print(F("1> "));
    callback_generic(recorded);
}

void callback2(const BitVector *recorded) {
    Serial.print(F("2> "));
    callback_generic(recorded);
}

void callback3(const BitVector *recorded) {
    Serial.print(F("3> "));
    callback_generic(recorded);
}

void callback4(const BitVector *recorded) {
    Serial.print(F("4> "));
    callback_generic(recorded);
}

void callback5(const BitVector *recorded) {
    Serial.print(F("5> "));
    callback_generic(recorded);
}

void callback6(const BitVector *recorded) {
    Serial.print(F("6> "));
    callback_generic(recorded);
    Serial.print(F("Callback executed only for the received code 4A9B9C9D\n"));
}

void callback7(const BitVector *recorded) {
    Serial.print(F("7> "));
    callback_generic(recorded);
    Serial.print(F("Callback executed only for the received code 4E9FA0A1\n"));
}

void callback8(const BitVector *recorded) {
    Serial.print(F("8> "));
    callback_generic(recorded);
}

RF_manager rf(PIN_RFINPUT, INT_RFINPUT);

void setup() {
    pinMode(PIN_RFINPUT, INPUT);
    Serial.begin(115200);

#ifdef NO_COMPACT_DURATIONS
    Serial.print(F("*** WARNING\n    "
            "NO_COMPACT_DURATIONS DEFINED, MEMORY FOOTPRINT WILL BE HIGHER\n"
            "    THIS IS A VALID SITUATION ONLY IF DEBUGGING\n"));
#endif

/*
 * The code below has been copy-pasted as is from the output of the program
 *   05_print_code_for_RF433recv_lib.ino
 * found in the folder
 *   examples/05_print_code_for_RF433recv_lib
 * of the library
 *   RF433any (https://github.com/sebmillet/RF433any)
*/

// First style: use register_callback() *after* registering receiver

        // FLO (no rolling code, 12-bit)
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
           12  // nb_bits
    );
    rf.register_callback(callback1, 2000);

// You can combine defining callbacks within register_Receiver and calling
// register_callback().
// Remember register_callback() *ALWAYS* applies to the *LAST* registered
// receiver.

// Second style: provide a callback at the time the receiver is registered

        // OTIO (no rolling code, 32-bit)
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
           32, // nb_bits
    callback2,
         2000
    );
    rf.register_callback(callback3, 1000);
    rf.register_callback(callback4, 200);

// The advantage of register_callback is that you can provide a code, to execute
// callback function only for a specific received code.

        // ADF (no rolling code, 32-bit)
    rf.register_Receiver(
        RFMOD_MANCHESTER, // mod
         5500, // initseq
            0, // lo_prefix
            0, // hi_prefix
            0, // first_lo_ign
         1166, // lo_short
            0, // lo_long
            0, // hi_short (0 => take lo_short)
            0, // hi_long  (0 => take lo_long)
            0, // lo_last
         5500, // sep
           32, // nb_bits
    callback5,
         2000
    );
    rf.register_callback(callback6, 2000,
            new BitVector(32, 4, 0x4A, 0x9B, 0x9C, 0x9D));
    rf.register_callback(callback7, 2000,
            new BitVector(32, 4, 0x4E, 0x9F, 0xA0, 0xA1));

        // FLO/R (rolling code, 72-bit, has a prefix)
    rf.register_Receiver(
        RFMOD_TRIBIT, // mod
        18000, // initseq
         1450, // lo_prefix
         1450, // hi_prefix
            0, // first_lo_ign
          450, // lo_short
          900, // lo_long
            0, // hi_short (0 => take lo_short)
            0, // hi_long  (0 => take lo_long)
         1400, // lo_last
        18000, // sep
           72, // nb_bits
    callback8,
         2000
    );

    Serial.print(F("Waiting for signal\n"));

    rf.set_opt_wait_free_433(false);
    rf.activate_interrupts_handler();
}

void loop() {
#ifdef SIMULATE_INTERRUPTS
    rf.wait_value_available();
    const Receiver* rec = rf.get_receiver_that_has_a_value();
    callback(rec->get_recorded());
    while (1)
        ;
#else
    rf.do_events();
#endif
}

// vim: ts=4:sw=4:tw=80:et
