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

//#define SIMULATE_INTERRUPTS

    // Use bytes to represent uint16_t. Yes it is possible! Obviously, to the
    // expense of precision. See compact function.
    //
    // * THIS CODE SHOULD ALWAYS WORK WITH COMPACT_DURATIONS DEFINED *
    //
    // The possibility *not* to compact durations is available for debugging
    // purposes.
#define COMPACT_DURATIONS

#ifndef COMPACT_DURATIONS
#pragma message("COMPACT_DURATIONS MACRO NOT DEFINED")
#endif

#define ARRAYSZ(a) (sizeof(a) / sizeof(*a))

#define ASSERT_OUTPUT_TO_SERIAL

#define delayed_assert(cond) { \
    if (!(cond)) { \
        delayed_assert = __LINE__; \
    } \
}

#define assert(cond) { \
    if (!(cond)) { \
        recv_ino_assert_failed(__LINE__); \
    } \
}

static void recv_ino_assert_failed(int line) {
#ifdef ASSERT_OUTPUT_TO_SERIAL
    Serial.print("\nrecv.ino:");
    Serial.print(line);
    Serial.println(": assertion failed, aborted.");
#endif
    while (1)
        ;
}


// * ********* ****************************************************************
// * BitVector ****************************************************************
// * ********* ****************************************************************

class BitVector {
    private:
        uint8_t* array;
        const byte target_nb_bits;
        const byte target_nb_bytes;
        byte nb_bits;
    public:
        BitVector(byte arg_target_nb_bits);
        virtual ~BitVector();

        virtual void reset();

        virtual void add_bit(byte v);

        virtual int get_nb_bits() const;
        virtual byte get_nb_bytes() const;
        virtual byte get_nth_byte(byte n) const;

        virtual char *to_str() const;
};

BitVector::BitVector(byte arg_target_nb_bits):
        target_nb_bits(arg_target_nb_bits),
        target_nb_bytes((arg_target_nb_bits + 7) >> 3),
        nb_bits(0) {
    assert(target_nb_bytes);
    array = (uint8_t*)malloc(target_nb_bytes);
}

BitVector::~BitVector() {
    if (array)
        free(array);
}

void BitVector::reset() {
    nb_bits = 0;
}

void BitVector::add_bit(byte v) {
    ++nb_bits;
    assert(nb_bits <= target_nb_bits);

    for (short i = target_nb_bytes - 1; i >= 0; --i) {

        byte b;
            // b must be 0 or 1, hence the !! sequences below
        if (i > 0) {
            b = !!(array[i - 1] & 0x80);
        } else {
            b = !!v;
        }

        array[i]= (array[i] << 1) | b;

    }
}

int BitVector::get_nb_bits() const {
    return nb_bits;
}

byte BitVector::get_nb_bytes() const {
    return (nb_bits + 7) >> 3;
}

    // Bit numbering starts at 0
byte BitVector::get_nth_byte(byte n) const {
    assert(n >= 0 && n < get_nb_bytes());
    return array[n];
}

    // *IMPORTANT*
    //   If no data got received, returns nullptr. So, you must test the
    //   returned value.
    //
    // *IMPORTANT (2)*
    //   The return value is malloc'd so caller must think of freeing it.
    //   For example:
    //     char *s = data_to_str_with_malloc(data);
    //     ...
    //     if (s)       // DON'T FORGET (s can be null)
    //         free(s); // DON'T FORGET! (if non-null, s must be freed)
char* BitVector::to_str() const {
    if (!get_nb_bits())
        return nullptr;

    byte nb_bytes = get_nb_bytes();

    char *ret = (char*)malloc(nb_bytes * 3);
    char tmp[3];
    int j = 0;
    for (int i = nb_bytes - 1; i >= 0 ; --i) {
        snprintf(tmp, sizeof(tmp), "%02x", get_nth_byte(i));
        ret[j] = tmp[0];
        ret[j + 1] = tmp[1];
        ret[j + 2] = (i > 0 ? ' ' : '\0');
        j += 3;
    }
    assert(j <= nb_bytes * 3);

    return ret;
}


// * ******* ******************************************************************
// * Automat ******************************************************************
// * ******* ******************************************************************

#define W_WAIT_SIGNAL    0
#define W_TERMINATE      1
#define W_CHECK_DURATION 2
#define W_RESET_BITS     3
#define W_ADD_ZERO       4
#define W_ADD_ONE        5
#define W_CHECK_BITS     6

// FIXME
//   Well, this one is "fix me if possible"!
//   arduino-builder does not compile the code when using instruction
//     typedef byte duration_t
//     (or:
//      typedef uint16_t duration_t
//     )
//   It'll say duration_t is unknown.
//   So I end up using #define, that looks weird.
//   avr-g++ works well with typedef.
#ifdef COMPACT_DURATIONS
#define duration_t byte
#else
#define duration_t uint16_t
#endif

struct auto_t {
    byte w;
    duration_t minval;
    duration_t maxval;
    byte next_if_w_true;
    byte next_if_w_false;
};

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

/*auto_t decoder_tribit_inverted[] = {

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
};*/

byte status = 0;
const auto_t *dec = &decoder_tribit[0];
const unsigned short dec_len = ARRAYSZ(decoder_tribit);
BitVector *recorded = nullptr;
bool has_value = false;
byte n = 32;

// compact() aims to represent 16-bit integers in 8-bit, to the cost of
// precision.
// The three sets (first one looses 4 bits, middle looses 7, last looses 12)
// have been chosen so that smaller durations don't loose too much precision.
//
// Any way, keep in mind Arduino timer produces values always multiple of 4,
// that shifts bit-loss by 2.
// For example, the first set (that looses 4 bits) actually really looses 2 bits
// of precision.
duration_t compact(uint16_t u) {
#ifndef COMPACT_DURATIONS
        // No compact -> compact() is a no-op
    return u;
#else
    if (u < 2048) {
        return u >> 4;
    }
    if (u < 17408) {
        return 128 + ((u - 2048) >> 7);
    }
    if (u < 46080)
        return 248 + ((u - 17408) >> 12);
    return 255;
#endif
}

// uncompact() is the opposite of compact(), yes!
// Left here in case tests are needed (not used in target code).
uint16_t uncompact(duration_t b) {
#ifndef COMPACT_DURATIONS
        // No compact -> uncompact() is a no-op
    return b;
#else
    uint16_t u = b;
    if (u < 128) {
        return u << 4;
    }
    u &= 0x7f;
    if (u < 120) {
        return (u << 7) + 2048;
    }
    return ((u - 120) << 12) + 17408;
#endif
}

void myset(byte line, uint16_t minv, uint16_t maxv, bool dont_compact = 0) {
    assert(decoder_tribit[line].minval == 251);
    assert(decoder_tribit[line].maxval == 251);

    if (dont_compact) {
        decoder_tribit[line].minval = minv;
        decoder_tribit[line].maxval = maxv;
    } else {
        decoder_tribit[line].minval = compact(minv);
        decoder_tribit[line].maxval = compact(maxv);
    }
}

inline bool w_compare(duration_t minval, duration_t maxval, duration_t val) {
    if (val < minval || val > maxval)
        return false;
    return true;
}

#ifdef SIMULATE_INTERRUPTS
uint16_t timings[] = {
//  0   ,  7064,
//  1160,   632,
//   456,  1344,
//  1156,   656,
//  1156,   660,
//   448,  1376,
//  1148,   684,
//   456,  1388,
//  1148,   652,
//   456,  1336,
//   452,  1340,
//  1152,   652,
//  1148,   672,
//   456,  1372,
//  1156,   692,
//   436,  1412,
//  1124,   676,
//   436,  1348,
//  1132,   656,
//  1148,   660,
//   448,  1372,
//  1148,   680,
//  1140,   688,
//   440,  1404,
//  1140,   664,
//   436,  1364,
//   424,  1376,
//   412,  1392,
//   412,  1396,
//   416,  1404,
//   416,  1412,
//   420,  1424,
//   420,  1348,
//   416,  7136,
//  1072,   720,
     0, 23916,
   716,   620,
  1376,  1312,
   688,   636,
  1380,  1300,
   720,  1280,
   724,  1292,
   712,  1288,
   716,   608,
  1368,  1320,
   676,  1352,
   644,   684,
  1328,  1368,
   648, 23924,
     0
};
const size_t timings_len = ARRAYSZ(timings);
byte timings_index = 0;
#endif

int delayed_assert = 0;
void check_delayed_assert() {
    if (!delayed_assert)
        return;
    Serial.print("delayed assert failed line ");
    Serial.print(delayed_assert);
    Serial.print("\n");
    while (1)
        ;
}

bool handle_int_overrun = false;
bool handle_int_busy = false;

void handle_int_receive() {
    static unsigned long last_t = 0;

    const unsigned long t = micros();
    unsigned long duration = t - last_t;
    last_t = t;

#ifdef SIMULATE_INTERRUPTS
    if (timings_index < timings_len) {
        duration = timings[timings_index];
        timings_index++;
    } else {
        return;
    }
    dbg("--");
#endif

    if (handle_int_busy) {
        handle_int_overrun = true;
        return;
    }

    if (handle_int_overrun) {
        duration = 0;
        handle_int_overrun = false;
    }
    handle_int_busy = true;
    sei();

    do {
        const auto_t *current = &dec[status];
        const byte w = current->w;

        bool r;
        switch (w) {
        case W_WAIT_SIGNAL:
            {
#ifdef SIMULATE_INTERRUPTS
                duration_t val = !(timings_index % 2);
#else
                duration_t val = (digitalRead(PIN_RFINPUT) == HIGH ? 1 : 0);
#endif
                r = w_compare(current->minval, current->maxval, val);
            }
            break;

        case W_TERMINATE:
            has_value = true;
            r = true;
            break;

        case W_CHECK_DURATION:
            r = w_compare(current->minval, current->maxval, compact(duration));
            break;

        case W_RESET_BITS:
            recorded->reset();
            r = true;
            break;

        case W_ADD_ZERO:
            recorded->add_bit(0);
            r = true;
            break;

        case W_ADD_ONE:
            recorded->add_bit(1);
            r = true;
            break;

        case W_CHECK_BITS:
            r = w_compare(current->minval, current->maxval,
                    recorded->get_nb_bits());
            break;

        default:
            assert(false);
        }

        byte next_status =
            (r ? current->next_if_w_true : current->next_if_w_false);
        delayed_assert(next_status < dec_len);

        dbgf("d = %lu, n = %d, status = %d, w = %d, next_status = %d",
                duration, recorded->get_nb_bits(), status, w, next_status);

        status = next_status;
    } while (dec[status].w != W_TERMINATE && dec[status].w != W_WAIT_SIGNAL);

    handle_int_busy = false;
}

void my_set_tribit() {
    myset(2, 5000, 65535);
    myset(5, 200, 800);
    myset(6, 900, 1500);
    myset(8, 900, 1500);
    myset(11, 200, 800);
    myset(13, 32, 32, 1);
    myset(15, 200, 1500);
    myset(17, 5000, 65535);
}

void my_set_tribit_inverted() {
    myset(2, 12000, 65535);
    myset(4, 200, 800);
    myset(7, 200, 800);
    myset(8, 900, 1500);
    myset(10, 900, 1500);
    myset(13, 200, 800);
    myset(15, 12, 12, 1);
    myset(17, 12000, 65535);

}

void setup() {
    pinMode(PIN_RFINPUT, INPUT);
    Serial.begin(115200);

#ifndef COMPACT_DURATIONS
    Serial.print("*** WARNING\n    "
            "COMPACT_DURATIONS NOT DEFINED, MEMORY FOOTPRINT WILL BE HIGHER\n"
            "    THIS IS A VALID SITUATION ONLY IF DEBUGGING\n");
#endif
    Serial.print("Waiting for signal\n");

    recorded = new BitVector(n);

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
}

void loop() {
#ifndef SIMULATE_INTERRUPTS
    attachInterrupt(INT_RFINPUT, &handle_int_receive, CHANGE);
#endif

    assert(recorded);
        // Defensive programming.
        // It is tempting to put it at the end of the loop() function, since the
        // first run (just after setup() execution) is useless. But putting it
        // here is more robust.
    recorded->reset();

    while (!has_value) {
        delay(1);
#ifdef SIMULATE_INTERRUPTS
        handle_int_receive();
#endif
        check_delayed_assert();
    }

#ifndef SIMULATE_INTERRUPTS
    detachInterrupt(INT_RFINPUT);
#endif

    char *printed_code = recorded->to_str();
    if (printed_code) {
        Serial.print("Received code: ");
        Serial.print(recorded->get_nb_bits());
        Serial.print(" bits: [");
        Serial.print(printed_code);
        Serial.print("]\n");
    }
    if (printed_code)
        free(printed_code);

    has_value = false;
    status = 0;
}

// vim: ts=4:sw=4:tw=80:et
