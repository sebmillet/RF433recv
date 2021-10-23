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

const auto_t automat_tribit[] PROGMEM = {

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

void myset(auto_t *dec, byte dec_len, byte line, uint16_t minv, uint16_t maxv) {
    assert(line < dec_len);
    assert(dec[line].minval == 251);
    assert(dec[line].maxval == 251);

    dec[line].minval = minv;
    dec[line].maxval = maxv;
}

#define RFMOD_TRIBIT          0
#define RFMOD_TRIBIT_INVERTED 1
#define RFMOD_MANCHESTER      2

struct RfSignal {
    byte mod;
    uint16_t initseq;
    uint16_t lo_prefix;
    uint16_t hi_prefix;
    uint16_t first_lo_ign;
    uint16_t lo_short;
    uint16_t lo_long;
    uint16_t hi_short;
    uint16_t hi_long;
    uint16_t lo_last;
    uint16_t sep;
    byte nb_bits;
};

void get_boundaries(uint16_t lo_short, uint16_t lo_long,
        duration_t& lo_short_inf, duration_t& lo_short_sup,
        duration_t& lo_long_inf, duration_t& lo_long_sup) {
    lo_short_inf = compact(lo_short >> 2);
    lo_short_sup = compact((lo_short + lo_long) >> 1);
    lo_long_inf = compact(lo_short_sup + 1);
    lo_long_sup = compact(lo_long + (lo_long >> 1));
}

void my_pgm_memcpy(void *dest, const void *src, size_t n) {
    const char *walker = (const char*)src;
    char *d = (char *)dest;
    for (size_t i = 0; i < n; ++i) {
        *(d++) = pgm_read_byte(walker++);
    }
}

auto_t* build_automat(byte mod, uint16_t initseq, uint16_t lo_prefix,
        uint16_t hi_prefix, uint16_t first_lo_ign, uint16_t lo_short,
        uint16_t lo_long, uint16_t hi_short, uint16_t hi_long, uint16_t lo_last,
        uint16_t sep, byte nb_bits) {

    assert((hi_short && hi_long) || (!hi_short && !hi_long));
    if (!hi_short && !hi_long) {
        hi_short = lo_short;
        hi_long = lo_long;
    }

    duration_t c_lo_short_inf;
    duration_t c_lo_short_sup;
    duration_t c_lo_long_inf;
    duration_t c_lo_long_sup;
    get_boundaries(lo_short, lo_long, c_lo_short_inf, c_lo_short_sup,
            c_lo_long_inf, c_lo_long_sup);
    duration_t c_hi_short_inf;
    duration_t c_hi_short_sup;
    duration_t c_hi_long_inf;
    duration_t c_hi_long_sup;
    get_boundaries(hi_short, hi_long, c_hi_short_inf, c_hi_short_sup,
            c_hi_long_inf, c_hi_long_sup);
    duration_t c_sep = compact(sep - (sep >> 2));
    duration_t c_l = (c_lo_long_sup >= c_hi_long_sup
            ? c_lo_long_sup : c_hi_long_sup);
    if (c_sep <= c_l)
        c_sep = c_l + 1;
    duration_t c_initseq = compact(initseq - (initseq >> 2));

    dbgf(   "c_lo_short_inf = %5u\n"
            "c_lo_short_sup = %5u\n"
            "c_lo_long_inf  = %5u\n"
            "c_lo_long_sup  = %5u\n",
            c_lo_short_inf, c_lo_short_sup, c_lo_long_inf, c_lo_long_sup);
    dbgf(   "c_hi_short_inf = %5u\n"
            "c_hi_short_sup = %5u\n"
            "c_hi_long_inf  = %5u\n"
            "c_hi_long_sup  = %5u\n",
            c_hi_short_inf, c_hi_short_sup, c_hi_long_inf, c_hi_long_sup);
    dbgf(   "c_sep          = %5u\n"
            "c_initseq      = %5u",
            c_sep, c_initseq);

    size_t sz = sizeof(automat_tribit);
    byte nb_elems = ARRAYSZ(automat_tribit);

    auto_t *pauto = (auto_t*)malloc(sz);

    switch (mod) {
    case RFMOD_TRIBIT:
        my_pgm_memcpy(pauto, automat_tribit, sz);
        myset(pauto, nb_elems, 2, c_initseq, compact(65535));
        myset(pauto, nb_elems, 5, c_lo_short_inf, c_lo_short_sup);
        myset(pauto, nb_elems, 6, c_lo_long_inf, c_lo_long_sup);
        myset(pauto, nb_elems, 8, c_hi_long_inf, c_hi_long_sup);
        myset(pauto, nb_elems, 11, c_hi_short_inf, c_hi_short_sup);
        myset(pauto, nb_elems, 13, nb_bits, nb_bits);
        myset(pauto, nb_elems, 15, c_lo_short_inf, c_lo_long_sup);
        myset(pauto, nb_elems, 17, c_sep, compact(65535));
        break;
    case RFMOD_TRIBIT_INVERTED:
        break;

    case RFMOD_MANCHESTER:
        break;

    default:
        assert(false);
    }

    return pauto;
}

void my_set_tribit_inverted() {
    byte sz = ARRAYSZ(decoder_tribit_inverted);
    myset(decoder_tribit_inverted, sz, 2, 12000, 65535);
    myset(decoder_tribit_inverted, sz, 4, 200, 800);
    myset(decoder_tribit_inverted, sz, 7, 200, 800);
    myset(decoder_tribit_inverted, sz, 8, 900, 1500);
    myset(decoder_tribit_inverted, sz, 10, 900, 1500);
    myset(decoder_tribit_inverted, sz, 13, 200, 800);
//    myset(decoder_tribit_inverted, sz, 15, 12, 12, 1);
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

    auto_t *decoder_otio = build_automat(
            RFMOD_TRIBIT, // mod
                    7000, // initseq
                       0, // lo_prefix
                       0, // hi_prefix
                       0, // first_lo_ign
                     620, // lo_short
                    1240, // lo_long
                       0, // hi_short (0 => take lo_short)
                       0, // hi_long  (0 => take hi_long)
                     620, // lo_last
                    7000, // sep
                      32  // nb_bits
            );
//    my_set_tribit_inverted();

    rf.register_Receiver(decoder_otio, ARRAYSZ(automat_tribit), 32);
//    rf.register_Receiver(decoder_tribit_inverted,
//            ARRAYSZ(decoder_tribit_inverted), 12);

    Serial.print(F("Waiting for signal\n"));
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
