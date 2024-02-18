// RF433recv.cpp

// See README.md about the purpose of this library

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

/*
  Schematic

  1. Arduino board. Tested with NANO and UNO.
  2. Radio Frequence 433Mhz RECEIVER like MX-RM-5V.

  RF433 RECEIVER data pin must be plugged on a board' digital PIN that can
  trigger interrupts.
  - On an UNO or NANO, this means D2 or D3.
  - Other boards will have different constraints. For example on ESP32, all GPIO
    pins can be configured as interrupts.
  This RECEIVER PIN is defined at the time a 'Track' object is created. This
  library does not set it at compile time.
  See file schema.fzz (Fritzing format) or schema.png, for a circuit example
  with receiver plugged on D2.
*/

#include "RF433recv.h"
#include <Arduino.h>

#define ASSERT_OUTPUT_TO_SERIAL

#define assert(cond) { \
    if (!(cond)) { \
        rf433recv_assert_failed(__LINE__); \
    } \
}

static void rf433recv_assert_failed(unsigned int line) {
#ifdef ASSERT_OUTPUT_TO_SERIAL
    Serial.print(F("\nRF433recv.cpp:"));
    Serial.print(line);
    Serial.println(F(": assertion failed, aborted."));
#endif
    while (1)
        ;
}

    // Could be in RF433recv.h (then, no need to declare it in the lib user
    // code), but I prefer to keep it out of symbols published by the lib...
#define ARRAYSZ(a) (sizeof(a) / sizeof(*a))

#if defined(ESP8266)
IRAM_ATTR
#endif
void handle_int_receive();


// * **************** *********************************************************
// * MeasureExecTimes *********************************************************
// * **************** *********************************************************

#ifdef DEBUG_EXEC_TIMES

// In this code area, we are debugging anyway -> we can use variables (like the
// buffer below) of big size, without optimizing anything.

static char serial_printf_buffer[200];

static void serial_printf(const char* fmt, ...)
     __attribute__((format(printf, 1, 2)));

static void serial_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(serial_printf_buffer, sizeof(serial_printf_buffer), fmt, args);
    va_end(args);

    serial_printf_buffer[sizeof(serial_printf_buffer) - 1] = '\0';
    Serial.print(serial_printf_buffer);
}

MeasureExecTimes::MeasureExecTimes(unsigned long int arg_reset_every):
        dmin(0),
        dmax(0),
        dtotal(0),
        count(0),
        reset_every(arg_reset_every) { }

MeasureExecTimes::MeasureExecTimes():MeasureExecTimes(0) { }

MeasureExecTimes::~MeasureExecTimes() { }

void MeasureExecTimes::reset() {
    dmin = 0;
    dmax = 0;
    dtotal = 0;
    count = 0;
}

void MeasureExecTimes::add(unsigned long int d) {
    if (reset_every && count == reset_every)
        reset();

    if (!count) {
        dmin = d;
        dmax = d;
    }

    if (d < dmin)
        dmin = d;
    if (d > dmax)
        dmax = d;

    dtotal += d;
    ++count;
}

void MeasureExecTimes::output_stats(const char *name) const {
    assert(name);
    unsigned long int davg = (count ? dtotal / count : 0);
    serial_printf("[%-4s] %7lu %7lu %7lu %7lu %7lu\n",
            name, dmin, davg, dmax, dtotal, count);
}

    // Why reset after 53 and 59 interrupts?
    // I am interested in the statistics of "coding" durations, that is, those
    // durations that are seen just before the Receiver says "I got data".
    // Managing a rolling buffer would be big and complex, I prefer to manage
    // like below: in average, half of the time r53 will have a count >= 27, and
    // half of the time r59 will have a count >= 30.
    // That means 75% of the time, we'll have a count number equal to, or above,
    // 27 (said differently: 75% of the time, the stats will be relevant).
    // I chose 53 and 59 (they are prime with one another) so that resetting of
    // their values is independant as much as possible (so I can say
    // "count >= 27" is 75% probable).
MeasureExecTimes measure_time_main;
MeasureExecTimes measure_time_r53(53);
MeasureExecTimes measure_time_r59(59);

unsigned int get_size_of_automats();

void output_measureexectimes_stats() {
    serial_printf("[%-4s] %7s %7s %7s %7s %7s\n",
            "CAT", "min", "avg", "max", "total", "count");
    measure_time_main.output_stats("MAIN");
    measure_time_r53.output_stats("R_53");
    measure_time_r59.output_stats("R_59");

    serial_printf("\n");

    measure_time_main.reset();
    measure_time_r53.reset();
    measure_time_r59.reset();
}

#endif


// * ********* ****************************************************************
// * BitVector ****************************************************************
// * ********* ****************************************************************

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

void BitVector::prepare_BitVector_construction(short arg_nb_bits,
        short arg_nb_bytes, short n) {
    assert(arg_nb_bits > 0);
    assert((arg_nb_bits + 7) >> 3 == arg_nb_bytes);
    assert(arg_nb_bytes == n);
    array = (uint8_t*)malloc(arg_nb_bytes);
    target_nb_bytes = arg_nb_bytes;
    nb_bits = arg_nb_bits;
}

BitVector::BitVector(short arg_nb_bits, short arg_nb_bytes, byte b0,
        byte b1) {
    prepare_BitVector_construction(arg_nb_bits, arg_nb_bytes, 2);
    array[1] = b0;
    array[0] = b1;
}

BitVector::BitVector(short arg_nb_bits, short arg_nb_bytes, byte b0, byte b1,
        byte b2) {
    prepare_BitVector_construction(arg_nb_bits, arg_nb_bytes, 3);
    array[2] = b0;
    array[1] = b1;
    array[0] = b2;
}

BitVector::BitVector(short arg_nb_bits, short arg_nb_bytes, byte b0, byte b1,
        byte b2, byte b3) {
    prepare_BitVector_construction(arg_nb_bits, arg_nb_bytes, 4);
    array[3] = b0;
    array[2] = b1;
    array[1] = b2;
    array[0] = b3;
}

BitVector::BitVector(short arg_nb_bits, short arg_nb_bytes, byte b0, byte b1,
        byte b2, byte b3, byte b4) {
    prepare_BitVector_construction(arg_nb_bits, arg_nb_bytes, 5);
    array[4] = b0;
    array[3] = b1;
    array[2] = b2;
    array[1] = b3;
    array[0] = b4;
}

BitVector::BitVector(short arg_nb_bits, short arg_nb_bytes, byte b0, byte b1,
        byte b2, byte b3, byte b4, byte b5) {
    prepare_BitVector_construction(arg_nb_bits, arg_nb_bytes, 6);
    array[5] = b0;
    array[4] = b1;
    array[3] = b2;
    array[2] = b3;
    array[1] = b4;
    array[0] = b5;
}

void BitVector::reset() {
    assert(array);
    nb_bits = 0;
    *array = 0;
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
byte BitVector::get_nth_bit(byte n) const {
    assert(n >= 0 && n < nb_bits);
    byte index = (n >> 3);
    byte bitread = (1 << (n & 0x07));
    return !!(array[index] & bitread);
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
    // *VERY IMPORTANT (2)* WARNING
    //   THE RETURN VALUE IS MALLOC'D SO CALLER MUST THINK OF FREEING IT.
    //   For example:
    //     char *s = to_str(data);
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

short BitVector::cmp(const BitVector *p) const {
    assert(p);
    short cmp_nb_bits = (get_nb_bits() > p->get_nb_bits());
    if (!cmp_nb_bits)
        cmp_nb_bits = -(get_nb_bits() < p->get_nb_bits());

    if (cmp_nb_bits)
        return cmp_nb_bits;

    for (int i = get_nb_bits() - 1; i >= 0; --i) {
        byte v1 = get_nth_bit(i);
        byte v2 = p->get_nth_bit(i);
        if (v1 > v2)
            return 1;
        if (v1 < v2)
            return -1;
    }

    return 0;
}


// * ****************** *******************************************************
// * compact, uncompact *******************************************************
// * ****************** *******************************************************

// compact() aims to represent 16-bit integers in 8-bit, to the cost of
// precision.
// The three sets (first one looses 4 bits, middle looses 7, last looses 12)
// have been chosen so that smaller durations don't loose too much precision.
// The higher the number, the more precision gets lost. This could be seen as
// 'the floating point number representation of the (very) poor man' (or,
// floating point numbers without... a floating point!)
//
// Any way, keep in mind Arduino timer produces values always multiple of 4,
// that shifts bit-loss by 2.
// For example, the first set (that looses 4 bits) actually really looses 2 bits
// of precision.
// In this archive, the file
//   test_compact.cpp
// contains code to test compact/uncompact results, for a few numbers.
duration_t compact(uint16_t u) {
#ifdef NO_COMPACT_DURATIONS
        // compact not activated -> compact() is a no-op
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
// Left here in case tests are needed (it is not used in release code).
//uint16_t uncompact(duration_t b) {
//#ifdef NO_COMPACT_DURATIONS
//         compact not activated -> uncompact() is a no-op
//    return b;
//#else
//    uint16_t u = b;
//    if (u < 128) {
//        return u << 4;
//    }
//    u &= 0x7f;
//    if (u < 120) {
//        return (u << 7) + 2048;
//    }
//    return ((u - 120) << 12) + 17408;
//#endif
//}


// * ********** ***************************************************************
// * autoline_t ***************************************************************
// * ********** ***************************************************************

// The below one corresponds to RFMOD_TRIBIT

const autoline_t automat_tribit[] PROGMEM = {

// Below, (T) means 'next status if test returns true'
//        (F) means 'next status if test returns false'

//    WHAT TO DO        MINVAL             MAXVAL           (T) (F)
    { W_WAIT_SIGNAL,    ADX_ONE,           ADX_ONE,          2,   0 }, //  0
    { W_TERMINATE,      ADX_UNDEF,         ADX_UNDEF,        1,  99 }, //  1

    { W_CHECK_DURATION, AD_INITSEQ_INF,    ADX_DMAX,
                                 AD_INDIRECT | AD_NEXT_PREFIX,    0 }, //  2

    { W_RESET_BITS,     ADX_UNDEF,         ADX_UNDEF,        4,  99 }, //  3

    { W_WAIT_SIGNAL,    ADX_ZERO,          ADX_ZERO,         5,   0 }, //  4
    { W_CHECK_DURATION, AD_LO_SHORT_INF,   AD_LO_SHORT_SUP,  7,   6 }, //  5
    { W_CHECK_DURATION, AD_LO_LONG_INF,    AD_LO_LONG_SUP,  10,   0 }, //  6

    { W_WAIT_SIGNAL,    ADX_ONE,           ADX_ONE,          8,   0 }, //  7
    { W_CHECK_DURATION, AD_HI_LONG_INF,    AD_HI_LONG_SUP,   9,   2 }, //  8
    { W_ADD_ZERO,       ADX_UNDEF,         ADX_UNDEF,       13,   0 }, //  9

    { W_WAIT_SIGNAL,    ADX_ONE,           ADX_ONE,         11,   0 }, // 10
    { W_CHECK_DURATION, AD_HI_SHORT_INF,   AD_HI_SHORT_SUP, 12,   2 }, // 11
    { W_ADD_ONE,        ADX_UNDEF,         ADX_UNDEF,       13,   0 }, // 12

    { W_CHECK_BITS,     AD_NB_BITS,        AD_NB_BITS,      14,   4 }, // 13
    { W_WAIT_SIGNAL,    ADX_ZERO,          ADX_ZERO,        15,   0 }, // 14
    { W_CHECK_DURATION, AD_LO_LAST_INF,    AD_LO_LAST_SUP,  16,   0 }, // 15
    { W_WAIT_SIGNAL,    ADX_ONE,           ADX_ONE,         17,   0 }, // 16
    { W_CHECK_DURATION, AD_SEP_INF,        ADX_DMAX,         1,   2 }, // 17

        // Used only if there is a prefix
    { W_WAIT_SIGNAL,    ADX_ZERO,          ADX_ZERO,        19,   0 }, // 18
    { W_CHECK_DURATION, AD_LO_PREFIX_INF, AD_LO_PREFIX_SUP, 20,   0 }, // 19
    { W_WAIT_SIGNAL,    ADX_ONE,           ADX_ONE,         21,   0 }, // 20
    { W_CHECK_DURATION, AD_HI_PREFIX_INF, AD_HI_PREFIX_SUP,  3,   2 }  // 21
};
#define TRIBIT_NB_ELEMS (ARRAYSZ(automat_tribit))

// The below one corresponds to RFMOD_TRIBIT_INVERTED

// IMPORTANT - FIXME (TODO actually)
// ***NOT TESTED WITH A PREFIX***
// IN REAL CONDITIONS, TESTED ONLY *WITHOUT* PREFIX
const autoline_t automat_tribit_inverted[] PROGMEM = {

// Below, (T) means 'next status if test returns true' and
//        (F) means 'next status if test returns false'.

//    WHAT TO DO        MINVAL             MAXVAL           (T) (F)
    { W_WAIT_SIGNAL,    ADX_ONE,           ADX_ONE,          2,   0 }, //  0
    { W_TERMINATE,      ADX_UNDEF,         ADX_UNDEF,        1,  99 }, //  1
    { W_CHECK_DURATION, AD_INITSEQ_INF,    ADX_DMAX,
                                  AD_INDIRECT | AD_NEXT_PREFIX,   0 }, //  2

    { W_WAIT_SIGNAL,    ADX_ZERO,          ADX_ZERO,         4,   0 }, //  3
    { W_CHECK_DURATION, AD_FIRST_LO_IGN_INF,
                                           AD_FIRST_LO_IGN_SUP,
                                                             5,   0 }, //  4

    { W_RESET_BITS,     ADX_UNDEF,         ADX_UNDEF,        6,  99 }, //  5

    { W_WAIT_SIGNAL,    ADX_ONE,           ADX_ONE,          7,   0 }, //  6
    { W_CHECK_DURATION, AD_HI_SHORT_INF,   AD_HI_SHORT_SUP,  9,   8 }, //  7
    { W_CHECK_DURATION, AD_HI_LONG_INF,    AD_HI_LONG_SUP,  12,   2 }, //  8

    { W_WAIT_SIGNAL,    ADX_ZERO,          ADX_ZERO,        10,   0 }, //  9
    { W_CHECK_DURATION, AD_LO_LONG_INF,    AD_LO_LONG_SUP,  11,
                                      AD_INDIRECT | AD_NEXT_SPECIAL }, // 10
    { W_ADD_ZERO,       ADX_UNDEF,         ADX_UNDEF,       15,   0 }, // 11

    { W_WAIT_SIGNAL,    ADX_ZERO,          ADX_ZERO,        13,   0 }, // 12
    { W_CHECK_DURATION, AD_LO_SHORT_INF,   AD_LO_SHORT_SUP, 14,   0 }, // 13
    { W_ADD_ONE,        ADX_UNDEF,         ADX_UNDEF,       15,   0 }, // 14

    { W_CHECK_BITS,     AD_NB_BITS,        AD_NB_BITS,      16,   6 }, // 15
    { W_WAIT_SIGNAL,    ADX_ONE,           ADX_ONE,         17,   0 }, // 16
    { W_CHECK_DURATION, AD_SEP_INF,        ADX_DMAX,         1,   2 }, // 17

        // Used only if there is a prefix
    { W_WAIT_SIGNAL,    ADX_ZERO,          ADX_ZERO,        19,   0 }, // 18
    { W_CHECK_DURATION, AD_LO_PREFIX_INF,  AD_LO_PREFIX_SUP,20,   0 }, // 19
    { W_WAIT_SIGNAL,    ADX_ONE,           ADX_ONE,         21,   0 }, // 20
    { W_CHECK_DURATION, AD_HI_PREFIX_INF,  AD_HI_PREFIX_SUP, 3,   2 }, // 21

    { W_CHECK_DURATION, AD_LO_SHORT_INF,   AD_LO_SHORT_SUP,  14,  0 }  // 22
};
#define TRIBIT_INVERTED_NB_ELEMS (ARRAYSZ(automat_tribit_inverted))

// The below one corresponds to RFMOD_MANCHESTER

const autoline_t automat_manchester[] PROGMEM = {

// Below, (T) means 'next status if test returns true' and
//        (F) means 'next status if test returns false'.

//    WHAT TO DO        MINVAL             MAXVAL           (T) (F)
    { W_WAIT_SIGNAL,    ADX_ONE,           ADX_ONE,          2,   0 }, //  0
    { W_TERMINATE,      ADX_UNDEF,         ADX_UNDEF,        1, 199 }, //  1
    { W_CHECK_DURATION, AD_INITSEQ_INF,    ADX_DMAX,         3,   0 }, //  2

    { W_WAIT_SIGNAL,    ADX_ZERO,          ADX_ZERO,         4,   0 }, //  3
    { W_CHECK_DURATION, AD_LO_SHORT_INF,   AD_LO_SHORT_SUP,  5,   0 }, //  4
    { W_WAIT_SIGNAL,    ADX_ONE,           ADX_ONE,          6,   0 }, //  5
    { W_CHECK_DURATION, AD_HI_SHORT_INF,   AD_HI_SHORT_SUP,  7,  32 }, //  6

    { W_RESET_BITS,     ADX_UNDEF,         ADX_UNDEF,        8, 199 }, //  7

    { W_WAIT_SIGNAL,    ADX_ZERO,          ADX_ZERO,         9,   0 }, //  8
    { W_CHECK_DURATION, AD_LO_SHORT_INF,   AD_LO_SHORT_SUP, 10,   0 }, //  9

    { W_WAIT_SIGNAL,    ADX_ONE,           ADX_ONE,         11,   0 }, // 10
    { W_CHECK_DURATION, AD_HI_SHORT_INF,   AD_HI_SHORT_SUP, 13,  12 }, // 11
    { W_CHECK_DURATION, AD_HI_LONG_INF,    AD_HI_LONG_SUP,  15,  29 }, // 12

    { W_ADD_ZERO,       ADX_UNDEF,         ADX_UNDEF,       14, 199 }, // 13
    { W_CHECK_BITS,     AD_NB_BITS,        AD_NB_BITS,      36,   8 }, // 14

    { W_ADD_ZERO,       ADX_UNDEF,         ADX_UNDEF,       16, 199 }, // 15
    { W_CHECK_BITS,     AD_NB_BITS,        AD_NB_BITS,      36,  17 }, // 16
    { W_WAIT_SIGNAL,    ADX_ZERO,          ADX_ZERO,        18,   0 }, // 17
    { W_CHECK_DURATION, AD_LO_SHORT_INF,   AD_LO_SHORT_SUP, 20,  19 }, // 18
    { W_CHECK_DURATION, AD_LO_LONG_INF,    AD_LO_LONG_SUP,  27,   0 }, // 19

    { W_ADD_ONE,        ADX_UNDEF,         ADX_UNDEF,       21, 199 }, // 20
    { W_CHECK_BITS,     AD_NB_BITS,        AD_NB_BITS,      34,  22 }, // 21
    { W_WAIT_SIGNAL,    ADX_ONE,           ADX_ONE,         23,   0 }, // 22
    { W_CHECK_DURATION, AD_HI_SHORT_INF,   AD_HI_SHORT_SUP, 24,   2 }, // 23
    { W_WAIT_SIGNAL,    ADX_ZERO,          ADX_ZERO,        25,   0 }, // 24
    { W_CHECK_DURATION, AD_LO_SHORT_INF,   AD_LO_SHORT_SUP, 20,  26 }, // 25
    { W_CHECK_DURATION, AD_LO_LONG_INF,    AD_LO_LONG_SUP,  27,   0 }, // 26

    { W_ADD_ONE,        ADX_UNDEF,         ADX_UNDEF,       28, 199 }, // 27
    { W_CHECK_BITS,     AD_NB_BITS,        AD_NB_BITS,      34,  10 }, // 28

    { W_CHECK_BITS,     ADX_NB_BITS_M1,    ADX_NB_BITS_M1,  30,   2 }, // 29
    { W_CHECK_DURATION, AD_SEP_INF,        ADX_DMAX,        31,   2 }, // 30
    { W_ADD_ZERO,       ADX_UNDEF,         ADX_UNDEF,        1, 199 }, // 31

    { W_CHECK_DURATION, AD_HI_LONG_INF,    AD_HI_LONG_SUP,  33,   2 }, // 32
    { W_RESET_BITS,     ADX_UNDEF,         ADX_UNDEF,       17, 199 }, // 33

    { W_WAIT_SIGNAL,    ADX_ONE,           ADX_ONE,         35,   0 }, // 34
    { W_CHECK_DURATION, AD_HI_SHORT_INF,   AD_HI_LONG_SUP,   2,   1 }, // 35

    { W_WAIT_SIGNAL,    ADX_ZERO,          ADX_ZERO,        37,   0 }, // 36
    { W_CHECK_DURATION, AD_LO_SHORT_INF,   AD_LO_LONG_SUP,   0,   1 }  // 37

};
#define MANCHESTER_NB_ELEMS (ARRAYSZ(automat_manchester))

//#define OUTPUT_SIZEOF_AUTOMATS_AT_COMPILE_TIME
#ifdef OUTPUT_SIZEOF_AUTOMATS_AT_COMPILE_TIME
    // Trick to output the sizeof of a structure by the compiler (AS AN ERROR)
    // found here:
    //   https://stackoverflow.com/questions/2008398/
    //     is-it-possible-to-print-out-the-size-of-a-c-class-at-compile-time
template<int s> struct Wow;
Wow<sizeof(automat_tribit)
    +sizeof(automat_tribit_inverted)
    +sizeof(automat_manchester)>
    wow;
#endif

    // TODO (?)
    // The boundaries are calculated so that a given signal length will be
    // identified as "short versus long" as follows:
    //   short <=> duration in [short / 4, avg(short, long)]
    //   long  <=> duration in [avg(short, long) + 1, long * 1.5]
    // (The use of compact numbers will also modify these boundaries a bit, but
    // this is another story.)
    //
    // This is a bit laxist. Stricter ranges could be:
    //   short <=> duration in [short * 0.75, short * 1.25]
    //   long  <=> duration in [long * 0.75, long * 1.25]
    //
    // As the author prefers the laxist way, providing stricter decoding would
    // require an additional argument when building automat, like for example...
    //   enum class DecodeMood {LAXIST, STRICT};
    // ...to change the calculation of boundaries accordingly.
    //
    // For now the author prefers to keep it simple, and always go the laxist
    // way.  ;-D
void get_boundaries(uint16_t sig_short, uint16_t sig_long, duration_t *pvalues,
        byte ad_idx_short_inf, byte ad_idx_short_sup,
        byte ad_idx_long_inf, byte ad_idx_long_sup) {

    if (sig_short != sig_long) {

        bool is_inverted = false;

            // Normally the short is... shorter than the long, yes!
            // But, some specs (in RCSwitch) work the other way round, and we
            // have to handle it gracefully.
        if (sig_short > sig_long) {
            is_inverted = true;
            uint16_t tmp = sig_short;
            sig_short = sig_long;
            sig_long = tmp;
        }

        pvalues[ad_idx_short_inf] = compact(sig_short >> 2);
        pvalues[ad_idx_short_sup] = compact((sig_short + sig_long) >> 1);
        pvalues[ad_idx_long_inf] = pvalues[ad_idx_short_sup] + 1;
        pvalues[ad_idx_long_sup] = compact(sig_long + (sig_long >> 1));

        if (is_inverted) {
            duration_t tmp_vinf = pvalues[ad_idx_short_inf];
            duration_t tmp_vsup = pvalues[ad_idx_short_sup];
            pvalues[ad_idx_short_inf] = pvalues[ad_idx_long_inf];
            pvalues[ad_idx_short_sup] = pvalues[ad_idx_long_sup];
            pvalues[ad_idx_long_inf] = tmp_vinf;
            pvalues[ad_idx_long_sup] = tmp_vsup;
        }

    } else {
            // This one is a bit special.
            // It is meant to handle cases like "RCSwitch protocol 8" where the
            // hi signal has no 'short' and 'long' durations, only the lo one
            // differs.
        pvalues[ad_idx_short_inf] = compact(sig_short >> 1);
        pvalues[ad_idx_short_sup] = compact(sig_short + (sig_short >> 1));
        pvalues[ad_idx_long_inf] = pvalues[ad_idx_short_inf];
        pvalues[ad_idx_long_sup] = pvalues[ad_idx_short_sup];
    }
}

// TODO
//   build_automat should be turned into autoexec_t constructor
autoexec_t* build_automat(byte mod, uint16_t initseq, uint16_t lo_prefix,
        uint16_t hi_prefix, uint16_t first_lo_ign, uint16_t lo_short,
        uint16_t lo_long, uint16_t hi_short, uint16_t hi_long, uint16_t lo_last,
        uint16_t sep, byte nb_bits) {

#ifdef DEBUG_AUTOMAT
    dbgf("== mod = %d, initseq = %u, lo_prefix = %u, hi_prefix = %u, "
            "first_lo_ign = %u\n", mod, initseq, lo_prefix, hi_prefix,
            first_lo_ign);
    dbgf("== lo_short = %u, lo_long = %u, hi_short = %u, hi_long = %u\n",
            lo_short, lo_long, hi_short, hi_long);
    dbgf("== lo_last = %u, sep = %u, nb_bits = %d\n",
            lo_last, sep, nb_bits);
#endif

    if (mod != RFMOD_MANCHESTER) {
        assert((lo_prefix && hi_prefix) || (!lo_prefix && !hi_prefix));
        assert((hi_short && hi_long) || (!hi_short && !hi_long));
        if (!hi_short && !hi_long) {
            hi_short = lo_short;
            hi_long = lo_long;
        }
    } else {
        assert(!lo_prefix && !hi_prefix);
        lo_long = lo_short << 1;
        if (!hi_short)
            hi_short = lo_short;
        hi_long = hi_short << 1;
    }

    autoexec_t *pax = new autoexec_t;
    duration_t *pvalues = pax->values;

    get_boundaries(lo_short, lo_long, pvalues,
            AD_LO_SHORT_INF, AD_LO_SHORT_SUP, AD_LO_LONG_INF, AD_LO_LONG_SUP);

    get_boundaries(hi_short, hi_long, pvalues,
            AD_HI_SHORT_INF, AD_HI_SHORT_SUP, AD_HI_LONG_INF, AD_HI_LONG_SUP);

    pvalues[AD_SEP_INF] = compact(sep - (sep >> 2));

    duration_t long_sup = (pvalues[AD_LO_LONG_SUP] >= pvalues[AD_HI_LONG_SUP]
            ? pvalues[AD_LO_LONG_SUP] : pvalues[AD_HI_LONG_SUP]);
    if (pvalues[AD_SEP_INF] <= long_sup)
        pvalues[AD_SEP_INF] = long_sup + 1;

    pvalues[AD_INITSEQ_INF] = compact(initseq - (initseq >> 2));

    if (lo_prefix) {
        pvalues[AD_LO_PREFIX_INF] = compact(lo_prefix - (lo_prefix >> 2));
        pvalues[AD_LO_PREFIX_SUP] = compact(lo_prefix + (lo_prefix >> 2));
        pvalues[AD_HI_PREFIX_INF] = compact(hi_prefix - (hi_prefix >> 2));
        pvalues[AD_HI_PREFIX_SUP] = compact(hi_prefix + (hi_prefix >> 2));
    } else {
            // Not necessary (unused if lo_prefix is zero)... but cleaner.
            // I put a value anyway, it is cleaner because in case the values
            // are tested (this without a doubt would be a bug), the bad
            // consequences will be repeatable, whatever it'll be.
            // Why choosing 32000?
            //   Why not?
        pvalues[AD_LO_PREFIX_INF] = compact(32000);
        pvalues[AD_LO_PREFIX_SUP] = compact(32000);
        pvalues[AD_HI_PREFIX_INF] = compact(32000);
        pvalues[AD_HI_PREFIX_SUP] = compact(32000);
    }

    pvalues[AD_LO_LAST_INF] =
        (lo_last ? compact(lo_last >> 1) : pvalues[AD_LO_SHORT_INF]);
    pvalues[AD_LO_LAST_SUP] =
        (lo_last ? compact(lo_last + (lo_last >> 1)) : pvalues[AD_LO_LONG_SUP]);

    pvalues[AD_FIRST_LO_IGN_INF] = compact(first_lo_ign >> 1);
    pvalues[AD_FIRST_LO_IGN_SUP] =
        compact(first_lo_ign + (first_lo_ign >> 1));

    pvalues[AD_NB_BITS] = nb_bits;

        // Will allow one day, to invert decoding.
        // For now, these two pvalues are clearly useless, as RF433recv API does
        // not provide a way to invert decoding.
        // Well, to be more precise, it does not provide a DIRECT way of
        // inverting decoding.
        // But, you can exchange short and long durations (in register_Receiver
        // call), this'll flip 0 and 1 decoded bits.
    pvalues[AD_BIT_0] = 0;
    pvalues[AD_BIT_1] = 1;

#ifdef DEBUG_AUTOMAT
    dbgf("c_lo_short_inf = %5u\n"
         "c_lo_short_sup = %5u\n"
         "c_lo_long_inf  = %5u\n"
         "c_lo_long_sup  = %5u\n",
         pvalues[AD_LO_SHORT_INF], pvalues[AD_LO_SHORT_SUP],
         pvalues[AD_LO_LONG_INF], pvalues[AD_LO_LONG_SUP]);
    dbgf("c_hi_short_inf = %5u\n"
         "c_hi_short_sup = %5u\n"
         "c_hi_long_inf  = %5u\n"
         "c_hi_long_sup  = %5u\n",
         pvalues[AD_HI_SHORT_INF], pvalues[AD_HI_SHORT_SUP],
         pvalues[AD_HI_LONG_INF], pvalues[AD_HI_LONG_SUP]);
    dbgf("c_sep_inf      = %5u\n"
         "c_initseq_inf  = %5u",
         pvalues[AD_SEP_INF], pvalues[AD_INITSEQ_INF]);
    dbgf("nb_bits        = %u\n",
         pvalues[AD_NB_BITS]);
#endif

    switch (mod) {

    case RFMOD_TRIBIT:

        pax->mat_len = TRIBIT_NB_ELEMS;
        pax->mat = automat_tribit;
        pvalues[AD_NEXT_PREFIX] = lo_prefix ? 18  : 3;

        break;

    case RFMOD_TRIBIT_INVERTED:

        pax->mat_len = TRIBIT_INVERTED_NB_ELEMS;
        pax->mat = automat_tribit_inverted;
            // As written earlier, not tested with a prefix
        pvalues[AD_NEXT_PREFIX] = lo_prefix ? 18  : 3;

            // If hi_short == hi_long, then the signal has the below shape:
            //
            //   First duration (tirets) is high signal
            //   Second duration (underscores) is low signal
            //     --- __     => hi 'short', lo short
            //     --- ____   => hi 'short', lo long
            //
            // 'short' is written in quotes, because it is neither short, nor
            // long: there is a unique duration, but the automat happens to
            // identify it as short.
            //
            // This means, at the time we 'identify' the hi signal duration, we
            // identify it (wrongly) as short and then, expect a long duration
            // thereafter, because regular tribit means encoding is made of ('hi
            // short then lo long' versus 'hi long then lo short').
            // However in this instance (hi_short == hi_long), we should instead
            // check if the following lo signal is short or long, to deduct the
            // bit coding.
            //
            // This is the purpose of the derivation below.
            //
            // FYI This coding corresponds to RCSwitch protocol 9.
        pvalues[AD_NEXT_SPECIAL] = (hi_short == hi_long ? 22 : 0);

        break;

    case RFMOD_MANCHESTER:

        assert(!lo_prefix);
        pax->mat_len = MANCHESTER_NB_ELEMS;
        pax->mat = automat_manchester;
        pvalues[AD_NEXT_PREFIX] = 255; // Not used
        break;

    default:
        assert(false);
    }

    return pax;
}


// * ******** *****************************************************************
// * Receiver *****************************************************************
// * ******** *****************************************************************

Receiver::Receiver(autoexec_t *arg_pax, byte arg_n):
        pax(arg_pax),
        n(arg_n),
        status(0),
        has_value(false),
        callback_head(nullptr),
        next(nullptr) {

    recorded = new BitVector(n);

    assert(pax);
    assert(n);
    assert(recorded);
}

Receiver::~Receiver() {
    if (pax)
        delete pax;
    if (recorded)
        delete recorded;
}

void Receiver::reset() {
    if (!recorded)
        return;

    status = 0;
    has_value = false;
    recorded->reset();
}

bool Receiver::w_compare(duration_t minval, duration_t maxval, duration_t val)
        const {

#ifdef DEBUG_AUTOMAT
    dbgf("Compare %u with [%u, %u]", val, minval, maxval);
#endif

    if (val < minval || val > maxval)
        return false;
    return true;
}

inline duration_t Receiver::get_val(byte idx) const {
    if (idx < AD_NB_FIELDS) {
        return pax->values[idx];
    } else if (idx == ADX_UNDEF) {
        return 42;   // Value returned does not matter;
    } else if (idx == ADX_ZERO) {
        return 0;
    } else if (idx == ADX_ONE) {
        return 1;
    } else if (idx == ADX_DMAX) {
        return compact(65535);
    } else if (idx == ADX_NB_BITS_M1) {
        return pax->values[AD_NB_BITS] - 1;
    } else {
        assert(false);
    }
    return -42;   // Never executed
}

void Receiver::process_signal(duration_t compact_signal_duration,
        byte signal_val) {
    const autoline_t *mat = pax->mat;
    byte new_w;
    do {
        const autoline_t *current = &mat[status];
        const byte w = pgm_read_byte(&current->w);

        duration_t minv = get_val(pgm_read_byte(&current->ad_field_idx_minval));
        duration_t maxv = get_val(pgm_read_byte(&current->ad_field_idx_maxval));

        bool r;
        switch (w) {
        case W_WAIT_SIGNAL:
            r = w_compare(minv, maxv, signal_val);
            break;

        case W_TERMINATE:
            has_value = true;
            r = true;
            break;

        case W_CHECK_DURATION:
            r = w_compare(minv, maxv, compact_signal_duration);
            break;

        case W_RESET_BITS:
            recorded->reset();
            r = true;
            break;

        case W_ADD_ZERO:
            recorded->add_bit(pax->values[AD_BIT_0]);
            r = true;
            break;

        case W_ADD_ONE:
            recorded->add_bit(pax->values[AD_BIT_1]);
            r = true;
            break;

        case W_CHECK_BITS:
            r = w_compare(minv, maxv, recorded->get_nb_bits());
            break;

        default:
            assert(false);
        }

        byte next_status = (r ?
            pgm_read_byte(&current->next_if_w_true)
                :
            pgm_read_byte(&current->next_if_w_false)
        );
        if (next_status & AD_INDIRECT)
            next_status = pax->values[next_status & ~AD_INDIRECT];

        assert(next_status < pax->mat_len);

#ifdef DEBUG_AUTOMAT
        dbgf("d = %u, n = %d, status = %d, w = %d, next_status = %d",
                compact_signal_duration, recorded->get_nb_bits(), status, w,
                next_status);
#endif

        status = next_status;
        new_w = pgm_read_byte(&mat[status].w);
    } while (new_w != W_TERMINATE && new_w != W_WAIT_SIGNAL);
}

void Receiver::attach(Receiver* ptr_rec) {
    assert(!next);
    next = ptr_rec;
}

callback_t* Receiver::get_callback_tail() const {
    const callback_t* pcb = callback_head;
    if (pcb) {
        while (pcb->next)
            pcb = pcb->next;
    }
    return (callback_t*)pcb;
}

void Receiver::add_callback(callback_t *pcb) {
    callback_t *pcb_tail = get_callback_tail();
    if (pcb_tail)
        pcb_tail->next = pcb;
    else
        callback_head = pcb;
}

byte Receiver::execute_callbacks() {
    uint32_t t0 = millis();

    byte ret = 0;

    callback_t *pcb = callback_head;
    while (pcb) {

        if (!pcb->min_delay_between_two_calls
                || !pcb->last_trigger
                || t0 >= pcb->last_trigger + pcb->min_delay_between_two_calls) {

            if (!pcb->pcode || !pcb->pcode->cmp(recorded)) {
                pcb->last_trigger = t0;
                pcb->func(recorded);
                ++ret;
            }
        }

        pcb = pcb->next;
    }
    reset();

    return ret;
}


// * ********** ***************************************************************
// * RF_manager ***************************************************************
// * ********** ***************************************************************

RF_manager::RF_manager(byte arg_pin_input_num, byte arg_int_num):
        int_num(arg_int_num),
        opt_wait_free_433_is_set(false),
        opt_wait_free_433_timeout(0),
        handle_int_receive_interrupts_is_set(false),
        first_decoder_that_has_a_value_resets_others(false),
        inactivate_interrupts_handler_when_a_value_has_been_received(false) {
    pin_input_num = arg_pin_input_num;
    head = nullptr;
    ++obj_count;

        // IT MAKES NO SENSE TO HAVE MORE THAN 1 RF_MANAGER
    assert(obj_count == 1);

}

RF_manager::RF_manager(byte arg_pin_input_num):
        RF_manager(arg_pin_input_num, digitalPinToInterrupt(arg_pin_input_num))
{
}

RF_manager::~RF_manager() { }

void RF_manager::set_opt_wait_free_433(bool v, uint32_t timeout) {
    opt_wait_free_433_is_set = v;
    opt_wait_free_433_timeout = timeout;
}

Receiver* RF_manager::get_tail() {
    const Receiver* ptr_rec = head;
    if (ptr_rec) {
        while (ptr_rec->get_next())
            ptr_rec = ptr_rec->get_next();
    }
    return (Receiver*)ptr_rec;
}

void RF_manager::register_Receiver(byte mod, uint16_t initseq,
        uint16_t lo_prefix, uint16_t hi_prefix, uint16_t first_lo_ign,
        uint16_t lo_short, uint16_t lo_long, uint16_t hi_short,
        uint16_t hi_long, uint16_t lo_last, uint16_t sep, byte nb_bits,
        void (*func)(const BitVector *recorded),
        uint32_t min_delay_between_two_calls) {

    autoexec_t *dex = build_automat(mod, initseq, lo_prefix, hi_prefix,
            first_lo_ign, lo_short, lo_long, hi_short, hi_long, lo_last, sep,
            nb_bits);

    Receiver *ptr_rec = new Receiver(dex, nb_bits);
    Receiver *tail = get_tail();
    if (!tail) {
        head = ptr_rec;
    } else {
        tail->attach(ptr_rec);
    }

    if (func)
        register_callback(func, min_delay_between_two_calls);
}

bool RF_manager::get_has_value() const {
    Receiver* ptr_rec = head;
    while (ptr_rec) {
        if (ptr_rec->get_has_value())
            return true;
        ptr_rec = ptr_rec->get_next();
    }
    return false;
}

Receiver* RF_manager::get_receiver_that_has_a_value() const {
    Receiver* ptr_rec = head;
    while (ptr_rec) {
        if (ptr_rec->get_has_value())
            return ptr_rec;
        ptr_rec = ptr_rec->get_next();
    }
    return nullptr;
}

void RF_manager::activate_interrupts_handler() {

    if (handle_int_receive_interrupts_is_set)
        return;

    handle_int_receive_interrupts_is_set = true;

#ifndef SIMULATE_INTERRUPTS
    attachInterrupt(int_num, &handle_int_receive, CHANGE);
#endif

}

void RF_manager::inactivate_interrupts_handler() {

    if (!handle_int_receive_interrupts_is_set)
        return;

#ifndef SIMULATE_INTERRUPTS
    detachInterrupt(int_num);
#endif

    handle_int_receive_interrupts_is_set = false;
}

void RF_manager::wait_value_available() {
    activate_interrupts_handler();
    while (!get_has_value()) {
        delay(1);
#ifdef SIMULATE_INTERRUPTS
        handle_int_receive();
#endif
    }
    inactivate_interrupts_handler();
}

void RF_manager::do_events() {
    bool has_waited_free_433 = false;

    bool deja_vu = false;
    bool reactivate_interrupts_handler_in_the_end = false;

    byte exec_count = 0;
    Receiver* ptr_rec = head;
    while (ptr_rec) {
        if (ptr_rec->get_has_value()) {

            if (inactivate_interrupts_handler_when_a_value_has_been_received
                    && !deja_vu) {
                deja_vu = true;
                reactivate_interrupts_handler_in_the_end =
                    handle_int_receive_interrupts_is_set;
                inactivate_interrupts_handler();
            }

            if (opt_wait_free_433_is_set) {
                if (!has_waited_free_433) {
                    wait_free_433();
                    has_waited_free_433 = true;
                }
            }

            exec_count += ptr_rec->execute_callbacks();

        }
        if (first_decoder_that_has_a_value_resets_others && exec_count) {
            ptr_rec = nullptr;
        } else {
            ptr_rec = ptr_rec->get_next();
        }
    }

    if (first_decoder_that_has_a_value_resets_others && exec_count) {

        cli();

        Receiver* ptr_rec = head;
        while (ptr_rec) {
            ptr_rec->reset();
            ptr_rec = ptr_rec->get_next();
        }

        sei();

    } else if (has_waited_free_433) {

            // Why reset everything when wait_free_433 is executed?
            // Because the timings are then completely messed up, and the
            // ongoing recording already done by any open receiver must be
            // restarted from scratch.

            // IMPORTANT
            // We have to clear interrupts, because an interrupt could occur
            // while we are resetting receivers, leading to unpredictable state
            // and behavior.
        cli();

        Receiver* ptr_rec = head;
        while (ptr_rec) {
            if (!ptr_rec->get_has_value()) {
                ptr_rec->reset();
            }
            ptr_rec = ptr_rec->get_next();
        }

        sei();

    }

    if (reactivate_interrupts_handler_in_the_end) {
        activate_interrupts_handler();
    }
}

void RF_manager::register_callback(void (*func)(const BitVector *recorded),
        uint32_t min_delay_between_two_calls, const BitVector *pcode) {

    Receiver *tail = get_tail();
    assert(tail); // A bit brutal... caller should know that register_callback
                  // can be done only after registering a Receiver.

    callback_t *pcb = new callback_t;
    pcb->pcode = pcode;
    pcb->func = func;
    pcb->min_delay_between_two_calls = min_delay_between_two_calls;
    pcb->last_trigger = 0;
    pcb->next = nullptr;

    tail->add_callback(pcb);
}

#if defined(ESP8266)
IRAM_ATTR
#endif
void RF_manager::ih_handle_interrupt_wait_free() {
    static unsigned long last_t = 0;

    const unsigned long t = micros();
    unsigned long d = t - last_t;
    last_t = t;

    if (d > 65535)
        d = 65535;

    short new_bit = (d >= 200 && d <= 25000);
    short old_bit = !!(IH_wait_free_last16 & 0x8000);
    IH_wait_free_last16 <<= 1;
    IH_wait_free_last16 |= new_bit;

    IH_wait_free_count_ok += new_bit;
    IH_wait_free_count_ok -= old_bit;
}

void RF_manager::wait_free_433() {

    bool save_handle_int_receive_interrupts_is_set =
        handle_int_receive_interrupts_is_set;

    inactivate_interrupts_handler();

    IH_wait_free_last16 = (uint16_t)0xffff;
    IH_wait_free_count_ok = 16;

    unsigned long int t0 = millis();

    attachInterrupt(int_num, &ih_handle_interrupt_wait_free, CHANGE);

        // 75% of the last 16 durations must be in the interval [200, 25000]
        // (that is, 12 'ok' out of 16 in total).
    while (IH_wait_free_count_ok >= 12 &&
              (!opt_wait_free_433_timeout
               || (millis() - t0 < opt_wait_free_433_timeout)
              )
          ) {
        ;
    }

    detachInterrupt(int_num);

    if (save_handle_int_receive_interrupts_is_set)
        activate_interrupts_handler();
}

byte RF_manager::pin_input_num = 255;
Receiver* RF_manager::head = nullptr;
byte RF_manager::obj_count = 0;

volatile short RF_manager::IH_wait_free_count_ok;
volatile uint16_t RF_manager::IH_wait_free_last16;


// * ***************** ********************************************************
// * Interrupt Handler ********************************************************
// * ***************** ********************************************************

#define treg1
#define treg2
#define treg3
#define treg4
#define treg5
#define treg6
#define treg7
#define treg8
#define treg9
#define treg10
#define treg11
#define treg12
#define treg13
#define treg14

#ifdef SIMULATE_INTERRUPTS
const uint16_t timings[] PROGMEM = {
#ifdef treg1
    0,    24116,    // reg1: 07 51 (tribit_inv, 12-bit)
    672,    612,
    1336,  1260,
    688,   1248,
    696,   1248,
    688,    608,
    1328,  1268,
    688,    608,
    1328,  1280,
    656,    636,
    1300,   636,
    1308,   636,
    1312,  1292,
    668,  65148,

                    // The below one is a repetition of the one above
    0,    24116,    // reg1: 07 51 (tribit_inv, 12-bit)
    672,    612,
    1336,  1260,
    688,   1248,
    696,   1248,
    688,    608,
    1328,  1268,
    688,    608,
    1328,  1280,
    656,    636,
    1300,   636,
    1308,   636,
    1312,  1292,
    668,  65148,
#endif

#ifdef treg2
    0,     7020,    // reg2: ad 15 (tribit, 16-bit)
    1292,   520,
    592,   1220,
    1288,   524,
    588,   1232,
    1284,   540,
    1272,   540,
    564,   1256,
    1244,   576,
    540,   1272,
    552,   1264,
    548,   1264,
    1272,   548,
    572,   1252,
    1260,   564,
    560,   1264,
    1260,   560,
    504,  65535,
#endif

#ifdef treg3
    0,    24100,    // reg3: d5 62 (tribit_inv, 16-bit)
    2064,  1432,
    468,   1424,
    468,    820,
    1068,  1436,
    476,    816,
    1052,  1464,
    420,    872,
    992,   1500,
    400,    900,
    1012,  1480,
    428,   1456,
    472,    820,
    1068,   840,
    1048,   848,
    1060,  1456,
    448,    844,
    1020, 55356,
#endif

#ifdef treg4
    0,    10044,    // reg4: d3 e5 (manchester, 16-bit)
    1144,  2308,
    1192,  1108,
    2348,  2288,
    2316,  1160,
    1128,  2328,
    1140,  1156,
    1148,  1152,
    1156,  1136,
    1156,  1136,
    2316,  1152,
    1144,  2328,
    2288,  2340,
    1140, 10032,

    0,    11236,    // reg4: 03 e0 (manchester, 16-bit)
    1148,  1148,
    1156,  1148,
    1148,  1148,
    1152,  1148,
    1144,  1156,
    1136,  1156,
    1148,  2312,
    1136,  1156,
    1144,  1156,
    1148,  1144,
    1148,  1156,
    2308,  1164,
    1148,  1160,
    1136,  1156,
    1148,  1164,
    1140, 52456,
#endif

#ifdef treg5
    0,     5560,    // reg5: 4e 9f a0 a1 (manchester, 32-bit)
    1136,  1156,
    1136,  2316,
    2324,  1156,
    1136,  2316,
    1136,  1164,
    1128,  1168,
    2296,  2316,
    2316,  1156,
    1136,  2316,
    1136,  1164,
    1136,  1164,
    1128,  1176,
    1124,  1176,
    1136,  1168,
    2304,  2324,
    2316,  1168,
    1132,  1176,
    1116,  1188,
    1116,  1184,
    1120,  2340,
    2308,  2328,
    2312,  1164,
    1128,  1176,
    1128,  1176,
    1128,  2352,
    1108,  5552,

    0,    11228,    // reg5: f0 55 aa 00 (manchester, 32-bit)
    1144,  2316,
    1148,  1156,
    1136,  1156,
    1140,  1156,
    2308,  1156,
    1136,  1164,
    1132,  1156,
    1136,  1156,
    1136,  2336,
    2292,  2328,
    2308,  2332,
    2296,  2336,
    1136,  1176,
    2296,  2336,
    2296,  2336,
    2296,  2336,
    2308,  1176,
    1120,  1176,
    1128,  1176,
    1128,  1184,
    1120,  1184,
    1128,  1172,
    1128,  1168,
    1136,  1176,
    1124, 30000,
#endif

#ifdef treg7
    0,    30000,    // reg7: 55 (manchester, 8-bit)
    1168,  1128,
    1156,  2304,
    2328,  2308,
    2316,  2324,
    2308,  2316,
    1140, 10048,
    1140,  1156,
    1136,  2328,
    2308,  2312,
    2316,  2316,
    2308,  2332,
    1136, 30000,

    0,    30000,    // reg7: 44 (manchester, 8-bit)
    1176,  1120,
    1184,  2284,
    2356,  1108,
    1176,  1120,
    1184,  2284,
    2328,  1140,
    1156, 30000,
                    // The below one MUST NOT match
    0,    30000,    // reg7: fake 44 (manchester, 8-bit)
    1176,  1120,
    1184,  2284,
    2356,  1108,
    1176,  1120,
    1184,  2284,
    2328,  1140,
    1156,  2284,    // ISSUE HERE (2284 instead of a separator like 30000)
    2328,  1140,
    1156, 30000,
#endif

#ifdef treg6
    0,   17884,     // reg6: 18 24 46 c1 d7 48 c8 66 08 (tribit, 72-bit)
    1432, 1416,
    424,   976,
    400,   992,
    396,   984,
    880,   500,
    896,   476,
    444,   912,
    508,   884,
    512,   868,
    532,   856,
    544,   848,
    984,   404,
    516,   888,
    484,   920,
    924,   492,
    408,   992,
    396,  1004,
    388,  1004,
    868,   524,
    396,   980,
    440,   924,
    492,   908,
    952,   440,
    964,   428,
    504,   884,
    976,   416,
    964,   436,
    476,   944,
    440,   976,
    404,  1004,
    380,  1020,
    368,  1020,
    860,   524,
    880,   504,
    896,   484,
    456,   924,
    944,   448,
    484,   904,
    964,   428,
    956,   444,
    932,   460,
    456,   960,
    888,   512,
    400,  1024,
    368,  1032,
    832,   560,
    360,  1012,
    388,   992,
    416,   964,
    916,   464,
    928,   452,
    476,   916,
    464,   936,
    932,   460,
    452,   944,
    448,   964,
    420,   992,
    384,  1020,
    840,   572,
    820,   568,
    352,  1020,
    372,  1004,
    888,   492,
    908,   464,
    460,   944,
    460,   928,
    456,   936,
    452,   944,
    456,   936,
    916,   496,
    416,   992,
    388,  1024,
    368,  1040,
   1304, 19376,
#endif

#ifdef treg8
    0,    4020,     // reg8: 03 e0 (manchester, 16-bit)
    456,   336,
    468,   320,
    448,   344,
    456,   332,
    460,   332,
    456,   320,
    476,   724,
    452,   332,
    456,   340,
    456,   320,
    464,   332,
    868,   340,
    456,   344,
    444,   348,
    436,   360,
    440,  4392,

    0,    4156,     // reg8: f3 0f (manchester, 16-bit)
    468,   732,
    476,   316,
    468,   316,
    476,   324,
    884,   312,
    476,   724,
    468,   328,
    868,   328,
    460,   340,
    456,   332,
    452,   752,
    456,   344,
    432,   364,
    452,   332,
    444,  3988,
#endif

#ifdef treg9
    0,   26144,     // reg9: 4d 2f (RCSwitch protocol 8)
    628,  3180,
    1416, 3188,
    620,  3188,
    616,  3188,
    1408, 3192,
    1420, 3188,
    620,  3192,
    1416, 3192,
    608,  3200,
    596,  3200,
    1416, 3188,
    628,  3180,
    1416, 3188,
    1416, 3196,
    1408, 3204,
    1416, 3200,
    608, 26108,
#endif

#ifdef treg10
    0,   26144,     // reg10: b2 d0 (RCSwitch protocol 8)
    628,  3180,
    1416, 3188,
    620,  3188,
    616,  3188,
    1408, 3192,
    1420, 3188,
    620,  3192,
    1416, 3192,
    608,  3200,
    596,  3200,
    1416, 3188,
    628,  3180,
    1416, 3188,
    1416, 3196,
    1408, 3204,
    1416, 3200,
    608, 26108,
#endif

#ifdef treg11
    0,     9652,    // reg11: 4d 2f (RCSwitch protocol 9)
    1432,  3180,
    1428,  3192,
    616,   3180,
    1420,  3180,
    1424,  3188,
    616,   3188,
    608,   3188,
    1416,  3188,
    608,   3200,
    1416,  3196,
    1408,  3200,
    608,   3188,
    1424,  3188,
    620,   3196,
    608,   3200,
    608,   3200,
    612,   9632,

    0,     9628,    // reg11: 4d 2f 7a e6 (RCSwitch protocol 9)
    1424,  3188,
    1424,  3188,
    628,  3188,
    1424,  3188,
    1412,  3200,
    632,  3184,
    620,  3188,
    1412,  3200,
    620,  3188,
    1424,  3188,
    1424,  3196,
    624,  3188,
    1412,  3200,
    608,  3200,
    620,  3188,
    628,  3188,
    608,  3200,
    1416,  3204,
    612,  3200,
    608,  3200,
    616,  3188,
    632,  3184,
    1416,  3200,
    608,  3200,
    1424,  3188,
    620,  3200,
    608,  3200,
    608,  3196,
    1428,  3188,
    1420,  3192,
    624,  3188,
    616,  3200,
    1404,  9644,
#endif

#ifdef treg12
    0,   8628,
    532, 3808,
    544, 1876,
    532, 1892,
    520, 3804,
    524, 3816,
    528, 1900,
    508, 3820,
    520, 1904,
    516, 1896,
    504, 1912,
    508, 3828,
    508, 1912,
    500, 3836,
    508, 3828,
    512, 1904,
    524, 3828,
    512, 1912,
    516, 1908,
    516, 1904,
    516, 3836,
    508, 1920,
    508, 1908,
    516, 1904,
    512, 1912,
    508, 3836,
    508, 1920,
    500, 3836,
    508, 1912,
    508, 1916,
    508, 1920,
    508, 3828,
    516, 1912,
    508, 3836,
    508, 3828,
    516, 1900,
    516, 3836,
    512, 3856,
    524, 8624,
#endif

#ifdef treg13
    0, 10228,
    280, 2544,
    272, 312,
    264, 1256,
    272, 1260,
    264, 324,
    268, 308,
    264, 1268,
    268, 312,
    268, 1252,
    272, 316,
    264, 1264,
    272, 316,
    276, 1260,
    272, 316,
    272, 1264,
    268, 312,
    268, 1268,
    272, 324,
    272, 1256,
    268, 1264,
    272, 324,
    264, 1264,
    268, 320,
    268, 1260,
    272, 324,
    256, 1268,
    272, 320,
    264, 328,
    264, 1260,
    264, 324,
    272, 1264,
    264, 1264,
    268, 324,
    264, 332,
    264, 1268,
    272, 320,
    264, 1272,
    264, 1268,
    264, 324,
    268, 320,
    260, 1264,
    272, 324,
    264, 1272,
    260, 320,
    264, 1272,
    264, 1268,
    264, 328,
    264, 1272,
    256, 332,
    264, 1272,
    256, 332,
    264, 1272,
    256, 328,
    264, 324,
    256, 1284,
    256, 328,
    264, 1272,
    264, 324,
    264, 1272,
    256, 332,
    264, 1272,
    256, 332,
    264, 1276,
    256, 332,
    260, 1272,
    264, 10220,
    276, 2560,
#endif

#ifdef treg14
0, 12096,
1328, 380,
464, 1164,
1316, 392,
1324, 396,
1296, 408,
1308, 404,
1308, 408,
1296, 416,
452, 1180,
448, 1184,
452, 1180,
452, 1184,
444, 1184,
448, 1184,
1300, 412,
1300, 416,
440, 1192,
440, 1188,
444, 1188,
440, 1192,
440, 1196,
432, 1196,
1288, 424,
428, 1204,
432, 12128,
#endif

    0, 0
};
const size_t timings_len = sizeof(timings) / sizeof(*timings);
size_t timings_index = 0;
bool has_read_all_timings() { return timings_index >= timings_len; }
#endif

static bool handle_int_busy = false;

struct sbuf_entry_t {
    byte signal_val;
    duration_t compact_signal_duration;
};
static sbuf_entry_t sbuf[BUFFER_SIGNALS_NB];
    // Keep in mind, we can't manage more than 128 values in the buffer (biggest
    // power of 2 that fits in a byte).
static byte sbuf_read_head = 0;
static byte sbuf_write_head = 0;

#if defined(ESP8266)
IRAM_ATTR
#endif
void handle_int_receive() {
    static unsigned long last_t = 0;

    const unsigned long t = micros();
    unsigned long signal_duration = t - last_t;
    last_t = t;

#ifdef SIMULATE_INTERRUPTS
    if (timings_index < timings_len) {
        signal_duration = pgm_read_word(&timings[timings_index]);

        timings_index++;
    } else {
        return;
    }
#ifdef DEBUG_AUTOMAT
    dbg("--");
#endif
#endif

    bool was_handle_int_busy = handle_int_busy;
    handle_int_busy = true;

#ifdef SIMULATE_INTERRUPTS
    byte signal_val = !(timings_index % 2);
#else
    byte signal_val =
        (digitalRead(RF_manager::get_pin_input_num()) == HIGH ? 1 : 0);
#endif

    duration_t compact_signal_duration = compact(signal_duration);

    byte next_sbuf_write_head = (sbuf_write_head + 1) & BUFFER_SIGNALS_MASK;
    if (next_sbuf_write_head == sbuf_read_head) {
            // There is a decision to take here: would we run out of space in
            // the buffer, we can either stop writing new signals, or, skip a
            // previously recorded signal.
            // For now, we decide to skip previously recorded signals.
        sbuf_read_head = (sbuf_read_head + 1) & BUFFER_SIGNALS_MASK;
    }
    sbuf[sbuf_write_head].signal_val = signal_val;
    sbuf[sbuf_write_head].compact_signal_duration = compact_signal_duration;
    sbuf_write_head = next_sbuf_write_head;

    if (!was_handle_int_busy) {
        while (sbuf_read_head != sbuf_write_head) {
            signal_val = sbuf[sbuf_read_head].signal_val;
            compact_signal_duration =
                sbuf[sbuf_read_head].compact_signal_duration;

            sei();

            Receiver *ptr_rec = RF_manager::get_head();
            while (ptr_rec) {

#ifdef DEBUG_AUTOMAT
                dbgf("\nptr_rec = %lu", (unsigned long)ptr_rec);
#endif

                ptr_rec->process_signal(compact_signal_duration, signal_val);
                ptr_rec = ptr_rec->get_next();
            }

            cli();
            sbuf_read_head = (sbuf_read_head + 1) & BUFFER_SIGNALS_MASK;
        }
        sei();
    }

#ifdef DEBUG_EXEC_TIMES
    const unsigned long d = micros() - t;
    measure_time_main.add(d);
    measure_time_r53.add(d);
    measure_time_r59.add(d);
#endif

    handle_int_busy = was_handle_int_busy;
}

// vim: ts=4:sw=4:tw=80:et
