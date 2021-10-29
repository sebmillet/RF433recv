// recv.ino

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
    Serial.print(F("\nrecv.ino:"));
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

void callback_2(const BitVector *recorded) {
    Serial.print(F("callback_2 called\n"));
}

void callback_3(const BitVector *recorded) {
    Serial.print(F("callback_3 called\n"));
}

void callback_man_1(const BitVector *recorded) {
    Serial.print(F("Manchester code 1 received\n"));
}

void callback_man_2(const BitVector *recorded) {
    Serial.print(F("Manchester code 2 received\n"));
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
    rf.register_callback(callback, 2000);

// Second style: provide a callback at the time the receiver is registered

        // FLO/R (rolling code, 72-bit, has a prefix)
    rf.register_Receiver(
        RFMOD_TRIBIT, // mod
        17888, // initseq
         1432, // lo_prefix
         1424, // hi_prefix
            0, // first_lo_ign
          474, // lo_short
          952, // lo_long
            0, // hi_short (0 => take lo_short)
            0, // hi_long  (0 => take lo_long)
         1400, // lo_last
        19324, // sep
           72, // nb_bits
        callback,
         2000
    );

// You can combine defining callbacks within register_Receiver and calling
// register_callback().
// Remember register_callback() *ALWAYS* applies to the *LAST* registered
// receiver.

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
        callback,
         2000
    );
    rf.register_callback(callback_2, 1000);
    rf.register_callback(callback_3, 200);

// The advantage of register_callback is that you can provide a code, to execute
// callback function only for a specific received code.

        // ADF (no rolling code, 32-bit)
    rf.register_Receiver(
        RFMOD_MANCHESTER, // mod
         8144, // initseq
            0, // lo_prefix
            0, // hi_prefix
            0, // first_lo_ign
         1166, // lo_short
         2330, // lo_long
            0, // hi_short (0 => take lo_short)
            0, // hi_long  (0 => take lo_long)
         2284, // lo_last
         8164, // sep
           32  // nb_bits
    );
    rf.register_callback(callback_man_1, 2000,
            new BitVector(32, 4, 0x40, 0x03, 0x89, 0x4e));
    rf.register_callback(callback_man_2, 2000,
            new BitVector(32, 4, 0x40, 0x03, 0x89, 0x4D));

    Serial.print(F("Waiting for signal\n"));

    rf.activate_interrupts_handler();

    assert(true); // Written to avoid a warning "unused function"
                  // FIXME (remove assert management code?)
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
