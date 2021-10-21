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


// * ******* ******************************************************************
// * Automat ******************************************************************
// * ******* ******************************************************************

auto_t decoder_tribit[] = {

// Below, (T) means 'next status if test returns true' and
//        (F) means 'next status if test returns false'.

//    WHAT TO DO      MINVAL MAXVAL (T)  (F)
    { W_WAIT_SIGNAL,       1,     1,  2,   0 }, //  0
    { W_TERMINATE,         0,     0,  1,  99 }, //  1
    { W_CHECK_DURATION,  251,   251,  3,   0 }, //  2

    { W_RESET_BITS,        0,     0,  4,  99 }, //  3

    { W_WAIT_SIGNAL,       0,     0,  5,   0 }, //  4
    { W_CHECK_DURATION,  251,   251,  7,   6 }, //  5
    { W_CHECK_DURATION,  251,   251, 10,   0 }, //  6

    { W_WAIT_SIGNAL,       1,     1,  8,   0 }, //  7
    { W_CHECK_DURATION,  251,   251,  9,   2 }, //  8
    { W_ADD_ZERO,          0,     0, 13,   0 }, //  9

    { W_WAIT_SIGNAL,       1,     1, 11,   0 }, // 10
    { W_CHECK_DURATION,  251,   251, 12,   2 }, // 11
    { W_ADD_ONE,           0,     0, 13,   0 }, // 12

    { W_CHECK_BITS,      251,   251, 14,   4 }, // 13
    { W_WAIT_SIGNAL,       0,     0, 15,   0 }, // 14
    { W_CHECK_DURATION,  251,   251, 16,   0 }, // 15
    { W_WAIT_SIGNAL,       1,     1, 17,   0 }, // 16
    { W_CHECK_DURATION,  251,   251,  1,   2 }  // 17
};

auto_t decoder_tribit_inverted[] = {

// Below, (T) means 'next status if test returns true' and
//        (F) means 'next status if test returns false'.

//    WHAT TO DO      MINVAL MAXVAL (T)  (F)
    { W_WAIT_SIGNAL,        1,     1,  2,   0 }, //  0
    { W_TERMINATE,          0,     0,  1,  99 }, //  1
    { W_CHECK_DURATION,   251,   251,  3,   0 }, //  2

    { W_WAIT_SIGNAL,        0,     0,  4,   0 }, //  3
    { W_CHECK_DURATION,   251,   251,  5,   0 }, //  4

    { W_RESET_BITS,         0,     0,  6,  99 }, //  5

    { W_WAIT_SIGNAL,        1,     1,  7,   0 }, //  6
    { W_CHECK_DURATION,   251,   251,  9,   8 }, //  7
    { W_CHECK_DURATION,   251,   251, 12,   2 }, //  8

    { W_WAIT_SIGNAL,        0,     0, 10,   0 }, //  9
    { W_CHECK_DURATION,   251,   251, 11,   0 }, // 10
    { W_ADD_ZERO,           0,     0, 15,   0 }, // 11

    { W_WAIT_SIGNAL,        0,     0, 13,   0 }, // 12
    { W_CHECK_DURATION,   251,   251, 14,   0 }, // 13
    { W_ADD_ONE,            0,     0, 15,   0 }, // 14

    { W_CHECK_BITS,       251,   251, 16,   6 }, // 15
    { W_WAIT_SIGNAL,        1,     1, 17,   0 }, // 16
    { W_CHECK_DURATION,   251,   251,  1,   2 }  // 17
};

void myset(auto_t *dec, byte dec_len, byte line, uint16_t minv, uint16_t maxv,
        bool dont_compact = 0) {
    assert(line < dec_len);
    assert(dec[line].minval == 251);
    assert(dec[line].maxval == 251);

    if (dont_compact) {
        dec[line].minval = minv;
        dec[line].maxval = maxv;
    } else {
        dec[line].minval = compact(minv);
        dec[line].maxval = compact(maxv);
    }
}

void my_set_tribit() {
    byte sz = ARRAYSZ(decoder_tribit);
    myset(decoder_tribit, sz, 2, 5000, 65535);
    myset(decoder_tribit, sz, 5, 200, 800);
    myset(decoder_tribit, sz, 6, 900, 1500);
    myset(decoder_tribit, sz, 8, 900, 1500);
    myset(decoder_tribit, sz, 11, 200, 800);
    myset(decoder_tribit, sz, 13, 32, 32, 1);
    myset(decoder_tribit, sz, 15, 200, 1500);
    myset(decoder_tribit, sz, 17, 5000, 65535);
}

void my_set_tribit_inverted() {
    byte sz = ARRAYSZ(decoder_tribit_inverted);
    myset(decoder_tribit_inverted, sz, 2, 12000, 65535);
    myset(decoder_tribit_inverted, sz, 4, 200, 800);
    myset(decoder_tribit_inverted, sz, 7, 200, 800);
    myset(decoder_tribit_inverted, sz, 8, 900, 1500);
    myset(decoder_tribit_inverted, sz, 10, 900, 1500);
    myset(decoder_tribit_inverted, sz, 13, 200, 800);
    myset(decoder_tribit_inverted, sz, 15, 12, 12, 1);
    myset(decoder_tribit_inverted, sz, 17, 12000, 65535);

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
    Serial.print(F("Waiting for signal\n"));

//    dbgf("compact(200) = %u => %u\n", compact(200),
//            uncompact(compact(200)));
//    dbgf("compact(800) = %u => %u\n", compact(800),
//            uncompact(compact(800)));
//    dbgf("compact(900) = %u => %u\n", compact(900),
//            uncompact(compact(900)));
//    dbgf("compact(1500) = %u => %u\n", compact(1500),
//            uncompact(compact(1500)));
//    dbgf("compact(5000) = %u => %u\n", compact(5000),
//            uncompact(compact(5000)));
//    dbgf("compact(10000) = %u => %u\n", compact(10000),
//            uncompact(compact(10000)));
//    dbgf("compact(20000) = %u => %u\n", compact(20000),
//            uncompact(compact(20000)));
//    dbgf("compact(30000) = %u => %u\n", compact(30000),
//            uncompact(compact(30000)));
//    dbgf("compact(40000) = %u => %u\n", compact(40000),
//            uncompact(compact(40000)));
//    dbgf("compact(46079) = %u => %u\n", compact(46079),
//            uncompact(compact(46079)));
//    dbgf("compact(46080) = %u => %u\n", compact(46080),
//            uncompact(compact(46080)));
//    dbgf("compact(46081) = %u => %u\n", compact(46081),
//            uncompact(compact(46081)));
//    dbgf("compact(50000) = %u => %u\n", compact(50000),
//            uncompact(compact(50000)));
//    dbgf("compact(60000) = %u => %u\n", compact(60000),
//            uncompact(compact(60000)));
//    dbgf("compact(65535) = %u => %u\n", compact(65535),
//            uncompact(compact(65535)));
//    while (1)
//        ;

    my_set_tribit();
//    my_set_tribit_inverted();

    rf.register_Receiver(decoder_tribit, ARRAYSZ(decoder_tribit), 32);
//    rf.register_Receiver(decoder_tribit_inverted,
//            ARRAYSZ(decoder_tribit_inverted), 12);
}

void loop() {
    rf.wait_value_available();

    Receiver *receiver = rf.get_receiver_that_has_a_value();
        // Condition must always be met, because we get_has_value() returned
        // true.
    assert(receiver);
    const BitVector *recorded = receiver->get_recorded();

    char *printed_code = recorded->to_str();
    if (printed_code) {
        Serial.print(F("Received code: "));
        Serial.print(recorded->get_nb_bits());
        Serial.print(F(" bits: ["));
        Serial.print(printed_code);
        Serial.print(F("]\n"));
    }
    if (printed_code)
        free(printed_code);
    receiver->reset();
}

// vim: ts=4:sw=4:tw=80:et
