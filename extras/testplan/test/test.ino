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

#define ARRAYSZ(a) (sizeof(a) / sizeof(*a))

extern const size_t timings_len;
extern size_t timings_index;

void callback(const BitVector *recorded) {
    static byte output_n = 0;

    ++output_n;

    Serial.print(F("output_n="));
    Serial.print(output_n);
    Serial.print(F(": "));
    Serial.print(F("code received: "));
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

#define BUILDFUNC_CALLBACK(n) \
void callback##n(const BitVector *recorded) { \
    Serial.print(F("reg")); \
    Serial.print(n); \
    Serial.print(F(": ")); \
    callback(recorded); \
}

BUILDFUNC_CALLBACK(1)
BUILDFUNC_CALLBACK(2)
BUILDFUNC_CALLBACK(3)
BUILDFUNC_CALLBACK(4)
BUILDFUNC_CALLBACK(5)
BUILDFUNC_CALLBACK(6)
BUILDFUNC_CALLBACK(7)
BUILDFUNC_CALLBACK(8)
BUILDFUNC_CALLBACK(9)
BUILDFUNC_CALLBACK(10)
BUILDFUNC_CALLBACK(11)
BUILDFUNC_CALLBACK(12)
BUILDFUNC_CALLBACK(13)

RF_manager rf(PIN_RFINPUT);

#ifdef DEBUG
void dbg_output_free_memory() {
    Serial.print(F("Free memory: "));
    Serial.print(freeMemory());
    Serial.print(F("\n"));
}
#else
#define dbg_output_free_memory(...)
#endif

void setup() {
    pinMode(PIN_RFINPUT, INPUT);
    Serial.begin(115200);

    dbg_output_free_memory();

#define reg1
#define reg2
#define reg3
#define reg4
#define reg5
#define reg6
#define reg7
#define reg8
#define reg9
#define reg10
#define reg11
#define reg12

#ifdef reg1
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
    callback1, // Callback when code received
            0
    );
#endif

#ifdef reg2
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
    callback2, // Callback when code received
            0
    );
#endif

#ifdef reg3
        // THIRD CODE, remotely inspired from FLO/R
    rf.register_Receiver(
        RFMOD_TRIBIT_INVERTED, // mod
        24000, // initseq
            0, // lo_prefix
            0, // hi_prefix
         2000, // first_lo_ign
          496, // lo_short
         1072, // lo_long
          836, // hi_short (0 => take lo_short)
         1436, // hi_long  (0 => take lo_long)
            0, // lo_last
        24000, // sep
           16, // nb_bits
    callback3,
            0
    );
#endif

#ifdef reg4
        // FOURTH ONE, inspired from ADF
    rf.register_Receiver(
        RFMOD_MANCHESTER, // mod
        10000, // initseq
            0, // lo_prefix
            0, // hi_prefix
            0, // first_lo_ign
         1166, // lo_short
            0, // lo_long
            0, // hi_short (0 => take lo_short)
            0, // hi_long  (0 => take lo_long)
         1164, // lo_last
        10000, // sep
           16, // nb_bits
    callback4,
            0
    );
#endif

#ifdef reg5
        // FIFTH ONE, Actual ADF (no rolling code, 32-bit)
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
            0
    );
#endif

#ifdef reg6
        // SIXTH ONE, FLO/R (rolling code, 72-bit, has a prefix)
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
    callback6,
            0
    );
#endif

#ifdef reg7
        // SEVENTH ONE, inspired from ADF
    rf.register_Receiver(
        RFMOD_MANCHESTER, // mod
        10000, // initseq
            0, // lo_prefix
            0, // hi_prefix
            0, // first_lo_ign
         1166, // lo_short
            0, // lo_long
            0, // hi_short (0 => take lo_short)
            0, // hi_long  (0 => take lo_long)
         1164, // lo_last
        10000, // sep
            8, // nb_bits
    callback7,
            0
    );
#endif

#ifdef reg8
        // EIGHTH ONE, inspired from ADF
    rf.register_Receiver(
        RFMOD_MANCHESTER, // mod
         4000, // initseq
            0, // lo_prefix
            0, // hi_prefix
            0, // first_lo_ign
          400, // lo_short
            0, // lo_long
            0, // hi_short (0 => take lo_short)
            0, // hi_long  (0 => take lo_long)
         1164, // lo_last
         4000, // sep
           16, // nb_bits
    callback8,
            0
    );
#endif

#ifdef reg9
        // NINETH ONE, comes from RCSwitch (protocol 8) -> the short duration is
        // longer than the long duration.
    rf.register_Receiver(
        RFMOD_TRIBIT, // mod
        26000,        // initseq
        0,            // lo_prefix
        0,            // hi_prefix
        0,            // first_lo_ign
        1400,         // lo_short
        600,          // lo_long
        3200,         // hi_short
        3200,         // hi_long
        600,          // lo_last
        26000,        // sep
        16,           // nb_bits
        callback9,
        0
    );
#endif

#ifdef reg10
        // TENTH ONE, comes from RCSwitch (protocol 8), inverted, meaning, the
        // short duration is shorter then the long duration.
    rf.register_Receiver(
        RFMOD_TRIBIT, // mod
        26000,        // initseq
        0,            // lo_prefix
        0,            // hi_prefix
        0,            // first_lo_ign
        600,          // lo_short
        1400,         // lo_long
        3200,         // hi_short
        3200,         // hi_long
        600,          // lo_last
        26000,        // sep
        16,           // nb_bits
        callback10,
        0
    );
#endif

#ifdef reg11
        // ELEVENTH ONE, comes from RCSwitch (protocol 9)
    rf.register_Receiver(
        RFMOD_TRIBIT_INVERTED, // mod
        9600,                  // initseq
        0,                     // lo_prefix
        0,                     // hi_prefix
        1400,                  // first_lo_ign
        600,                   // lo_short
        1400,                  // lo_long
        3200,                  // hi_short
        3200,                  // hi_long
        0,                     // lo_last
        9600,                  // sep
        16,                    // nb_bits
        callback11,
        0
    );

        // 12TH ONE, comes from RCSwitch (protocol 9)
    rf.register_Receiver(
        RFMOD_TRIBIT_INVERTED, // mod
        9600,                  // initseq
        0,                     // lo_prefix
        0,                     // hi_prefix
        1400,                  // first_lo_ign
        600,                   // lo_short
        1400,                  // lo_long
        3200,                  // hi_short
        3200,                  // hi_long
        0,                     // lo_last
        9600,                  // sep
        32,                    // nb_bits
        callback12,
        0
    );
#endif

#ifdef reg12
    rf.register_Receiver(
        RFMOD_TRIBIT_INVERTED, // mod
        8550,                  // initseq
        0,                     // lo_prefix
        0,                     // hi_prefix
        500,                   // first_lo_ign
        500,                   // lo_short
        500,                   // lo_long
        1900,                  // hi_short
        3800,                  // hi_long
        0,                     // lo_last
        8550,                  // sep
        37,                    // nb_bits
        callback13,
        1000);
#endif

    rf.set_opt_wait_free_433(false);
    rf.activate_interrupts_handler();

    dbg_output_free_memory();
}

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

    dbg_output_free_memory();

    while (1)
        ;

}

// vim: ts=4:sw=4:tw=80:et
