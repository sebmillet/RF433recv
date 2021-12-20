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
  trigger interrupts, that is, D2 or D3.
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

void handle_int_receive();

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
//
// Any way, keep in mind Arduino timer produces values always multiple of 4,
// that shifts bit-loss by 2.
// For example, the first set (that looses 4 bits) actually really looses 2 bits
// of precision.
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
// Left here in case tests are needed (not used in target code).
// FIXME
//   Comment it for production code (not used normally in production)
uint16_t uncompact(duration_t b) {
#ifdef NO_COMPACT_DURATIONS
        // compact not activated -> uncompact() is a no-op
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


// * ****** *******************************************************************
// * auto_t *******************************************************************
// * ****** *******************************************************************

#ifdef NO_PROGMEM_FOR_AUTOMAT_TEMPLATES
#define MY_PROGMEM
#else
#define MY_PROGMEM PROGMEM
#endif

    // The below one corresponds to RFMOD_TRIBIT
const auto_t automat_tribit[] MY_PROGMEM = {

// Below, (T) means 'next status if test returns true' and
//        (F) means 'next status if test returns false'.

//    WHAT TO DO      MINVAL MAXVAL (T)  (F)
    { W_WAIT_SIGNAL,       1,     1,  2,   0 }, //  0
    { W_TERMINATE,         0,     0,  1,  99 }, //  1
    { W_CHECK_DURATION,  251,   251, 18,   0 }, //  2

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
    { W_CHECK_DURATION,  251,   251,  1,   2 }, // 17

        // Used only if there is a prefix
    { W_WAIT_SIGNAL,       0,     0, 19,   0 }, // 18
    { W_CHECK_DURATION,  251,   251, 20,   0 }, // 19
    { W_WAIT_SIGNAL,       1,     1, 21,   0 }, // 20
    { W_CHECK_DURATION,  251,   251,  3,   2 }  // 21
};
    // [CMT314159]
    // Below, the number 4 showing up corresponds to the size of the prefix
    // management part of the automat (from status 18 until status 21 => 4
    // elements).
    // By default, the status number 2 points to (if success) to status number
    // 18, meaning, by default the automat *does* expect a prefix. If there is
    // no prefix (most frequent case), the next status of status number 2 must
    // be set (if success) to 3.
    //
    // This is why, later in this code, you have a line
    //     pauto[2].next_if_w_true = 3;
    // if there is no prefix.
    //
#define TRIBIT_NB_BYTES_WITH_PREFIX (sizeof(automat_tribit))
#define TRIBIT_NB_BYTES_WITHOUT_PREFIX \
    (TRIBIT_NB_BYTES_WITH_PREFIX - 4 * sizeof(*automat_tribit))
#define TRIBIT_NB_ELEMS_WITH_PREFIX (ARRAYSZ(automat_tribit))
#define TRIBIT_NB_ELEMS_WITHOUT_PREFIX (TRIBIT_NB_ELEMS_WITH_PREFIX - 4)

// IMPORTANT - FIXME
// ***NOT TESTED WITH A PREFIX***
// IN REAL CONDITIONS, TESTED ONLY *WITHOUT* PREFIX
const auto_t automat_tribit_inverted[] MY_PROGMEM = {

// Below, (T) means 'next status if test returns true' and
//        (F) means 'next status if test returns false'.

//    WHAT TO DO       MINVAL MAXVAL (T)  (F)
    { W_WAIT_SIGNAL,        1,     1,  2,   0 }, //  0
    { W_TERMINATE,          0,     0,  1,  99 }, //  1
    { W_CHECK_DURATION,   251,   251, 18,   0 }, //  2

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
    { W_CHECK_DURATION,   251,   251,  1,   2 }, // 17

        // Used only if there is a prefix
    { W_WAIT_SIGNAL,       0,     0, 19,   0 }, // 18
    { W_CHECK_DURATION,  251,   251, 20,   0 }, // 19
    { W_WAIT_SIGNAL,       1,     1, 21,   0 }, // 20
    { W_CHECK_DURATION,  251,   251,  3,   2 }  // 21
};
    // See comment [CMT314159] above about where do such formula come from
#define TRIBIT_INVERTED_NB_BYTES_WITH_PREFIX (sizeof(automat_tribit_inverted))
#define TRIBIT_INVERTED_NB_BYTES_WITHOUT_PREFIX \
    (TRIBIT_INVERTED_NB_BYTES_WITH_PREFIX - \
     4 * sizeof(*automat_tribit_inverted))
#define TRIBIT_INVERTED_NB_ELEMS_WITH_PREFIX (ARRAYSZ(automat_tribit_inverted))
#define TRIBIT_INVERTED_NB_ELEMS_WITHOUT_PREFIX \
    (TRIBIT_INVERTED_NB_ELEMS_WITH_PREFIX - 4)

const auto_t automat_manchester[] MY_PROGMEM = {

// Below, (T) means 'next status if test returns true' and
//        (F) means 'next status if test returns false'.

//    WHAT TO DO       MINVAL MAXVAL (T)  (F)
    { W_WAIT_SIGNAL,        1,     1,  2,   0 }, //  0
    { W_TERMINATE,          0,     0,  1, 199 }, //  1
    { W_CHECK_DURATION,   251,   251,  3,   0 }, //  2

    { W_WAIT_SIGNAL,        0,     0,  4,   0 }, //  3
    { W_CHECK_DURATION,   251,   251,  5,   0 }, //  4
    { W_WAIT_SIGNAL,        1,     1,  6,   0 }, //  5
    { W_CHECK_DURATION,   251,   251,  7,  32 }, //  6

    { W_RESET_BITS,         0,     0,  8, 199 }, //  7

    { W_WAIT_SIGNAL,        0,     0,  9,   0 }, //  8
    { W_CHECK_DURATION,   251,   251, 10,   0 }, //  9

    { W_WAIT_SIGNAL,        1,     1, 11,   0 }, // 10
    { W_CHECK_DURATION,   251,   251, 13,  12 }, // 11
    { W_CHECK_DURATION,   251,   251, 15,  29 }, // 12

    { W_ADD_ZERO,           0,     0, 14, 199 }, // 13
    { W_CHECK_BITS,       251,   251, 36,   8 }, // 14

    { W_ADD_ZERO,           0,     0, 16, 199 }, // 15
    { W_CHECK_BITS,       251,   251, 36,  17 }, // 16
    { W_WAIT_SIGNAL,        0,     0, 18,   0 }, // 17
    { W_CHECK_DURATION,   251,   251, 20,  19 }, // 18
    { W_CHECK_DURATION,   251,   251, 27,   0 }, // 19

    { W_ADD_ONE,            1,     1, 21, 199 }, // 20
    { W_CHECK_BITS,       251,   251, 34,  22 }, // 21
    { W_WAIT_SIGNAL,        1,     1, 23,   0 }, // 22
    { W_CHECK_DURATION,   251,   251, 24,   2 }, // 23
    { W_WAIT_SIGNAL,        0,     0, 25,   0 }, // 24
    { W_CHECK_DURATION,   251,   251, 20,  26 }, // 25
    { W_CHECK_DURATION,   251,   251, 27,   0 }, // 26

    { W_ADD_ONE,            0,     0, 28, 199 }, // 27
    { W_CHECK_BITS,       251,   251, 34,  10 }, // 28

    { W_CHECK_BITS,       251,   251, 30,   2 }, // 29
    { W_CHECK_DURATION,   251,   251, 31,   2 }, // 30
    { W_ADD_ZERO,           0,     0,  1, 199 }, // 31

    { W_CHECK_DURATION,   251,   251, 33,   2 }, // 32
    { W_RESET_BITS,         0,     0, 17, 199 }, // 33

    { W_WAIT_SIGNAL,        1,     1, 35,   0 }, // 34
    { W_CHECK_DURATION,   251,   251,  2,   1 }, // 35

    { W_WAIT_SIGNAL,        0,     0, 37,   0 }, // 36
    { W_CHECK_DURATION,   251,   251,  0,   1 }  // 37

};
#define MANCHESTER_NB_BYTES_WITHOUT_PREFIX (sizeof(automat_manchester))
#define MANCHESTER_NB_ELEMS_WITHOUT_PREFIX (ARRAYSZ(automat_manchester))

void myset(auto_t *dec, byte dec_len, byte line, uint16_t minv, uint16_t maxv) {
    assert(line < dec_len);
    assert(dec[line].minval == 251);
    assert(dec[line].maxval == 251);

    dec[line].minval = minv;
    dec[line].maxval = maxv;
}

    // TODO (?)
    // The boundaries are calculated so that a given signal length will be
    // identified as "short versus long" as follows:
    //   short <=> duration in [short / 4, avg(short, long)]
    //   long  <=> duration in [avg(short, long) + 1, long * 1.5]
    // (The use of compact numbers will also modify these bounadires a bit, but
    // this is another story.)
    //
    // This is a bit laxist. Stricter ranges could be:
    //   short <=> duration in [short * 0.75, short * 1.25]
    //   long  <=> duration in [long * 0.75, long * 1.25]
    //
    // As the author prefers the laxist way, providing stricter decoding would
    // require an additional argument in build_automat(), like for example...
    //   enum class DecodeMood {LAXIST, STRICT};
    // ...to change the calculation of boundaries accordingly.
    //
    // For now the author prefers to keep it simple, and always go the laxist
    // way.  ;-D
void get_boundaries(uint16_t lo_short, uint16_t lo_long,
        duration_t& lo_short_inf, duration_t& lo_short_sup,
        duration_t& lo_long_inf, duration_t& lo_long_sup) {
    lo_short_inf = compact(lo_short >> 2);
    lo_short_sup = compact((lo_short + lo_long) >> 1);
    lo_long_inf = compact(lo_short_sup + 1);
    lo_long_sup = compact(lo_long + (lo_long >> 1));
}

void my_pgm_memcpy(void *dest, const void *src, size_t n) {
#ifdef NO_PROGMEM_FOR_AUTOMAT_TEMPLATES
    memcpy(dest, src, n);
#else
    const char *walker = (const char*)src;
    char *d = (char *)dest;
    for (size_t i = 0; i < n; ++i) {
        *(d++) = pgm_read_byte(walker++);
    }
#endif
}

auto_t* build_automat(byte mod, uint16_t initseq, uint16_t lo_prefix,
        uint16_t hi_prefix, uint16_t first_lo_ign, uint16_t lo_short,
        uint16_t lo_long, uint16_t hi_short, uint16_t hi_long, uint16_t lo_last,
        uint16_t sep, byte nb_bits, byte *pnb_elems) {

    dbgf("== mod = %d, initseq = %u, lo_prefix = %u, hi_prefix = %u, "
            "first_lo_ign = %u\n", mod, initseq, lo_prefix, hi_prefix,
            first_lo_ign);
    dbgf("== lo_short = %u, lo_long = %u, hi_short = %u, hi_long = %u\n",
            lo_short, lo_long, hi_short, hi_long);
    dbgf("== lo_last = %u, sep = %u, nb_bits = %d, *pnb_elems = %d\n",
            lo_last, sep, nb_bits, *pnb_elems);

    if (mod != RFMOD_MANCHESTER) {
        assert((lo_prefix && hi_prefix) || (!lo_prefix && !hi_prefix));
        assert((hi_short && hi_long) || (!hi_short && !hi_long));
        if (!hi_short && !hi_long) {
            hi_short = lo_short;
            hi_long = lo_long;
        }
    } else {
        assert(!lo_prefix && !hi_prefix);
        assert(!lo_long);
        assert(!hi_long);
        lo_long = lo_short << 1;
        if (!hi_short)
            hi_short = lo_short;
        hi_long = hi_short << 1;
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

    duration_t c_lo_prefix_inf;
    duration_t c_lo_prefix_sup;
    duration_t c_hi_prefix_inf;
    duration_t c_hi_prefix_sup;
    if (lo_prefix) {
        c_lo_prefix_inf = compact(lo_prefix - (lo_prefix >> 2));
        c_lo_prefix_sup = compact(lo_prefix + (lo_prefix >> 2));
        c_hi_prefix_inf = compact(hi_prefix - (hi_prefix >> 2));
        c_hi_prefix_sup = compact(hi_prefix + (hi_prefix >> 2));
    }

    duration_t c_lo_last_inf =
        (lo_last ? compact(lo_last >> 1) : c_lo_short_inf);
    duration_t c_lo_last_sup =
        (lo_last ? compact(lo_last + (lo_last >> 1)) : c_lo_long_sup);

    duration_t c_first_lo_ign_inf = compact(first_lo_ign >> 1);
    duration_t c_first_lo_ign_sup = compact(first_lo_ign + (first_lo_ign >> 1));

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

    size_t sz;
    auto_t *pauto;

    switch (mod) {

    case RFMOD_TRIBIT:

        sz = (lo_prefix
                ? TRIBIT_NB_BYTES_WITH_PREFIX : TRIBIT_NB_BYTES_WITHOUT_PREFIX);
        *pnb_elems = (lo_prefix
                ? TRIBIT_NB_ELEMS_WITH_PREFIX : TRIBIT_NB_ELEMS_WITHOUT_PREFIX);

        pauto = (auto_t*)malloc(sz);
        my_pgm_memcpy(pauto, automat_tribit, sz);

        myset(pauto, *pnb_elems, 2, c_initseq, compact(65535));
        myset(pauto, *pnb_elems, 5, c_lo_short_inf, c_lo_short_sup);
        myset(pauto, *pnb_elems, 6, c_lo_long_inf, c_lo_long_sup);
        myset(pauto, *pnb_elems, 8, c_hi_long_inf, c_hi_long_sup);
        myset(pauto, *pnb_elems, 11, c_hi_short_inf, c_hi_short_sup);
        myset(pauto, *pnb_elems, 13, nb_bits, nb_bits);
        myset(pauto, *pnb_elems, 15, c_lo_last_inf, c_lo_last_sup);
        myset(pauto, *pnb_elems, 17, c_sep, compact(65535));

        if (lo_prefix) {
            myset(pauto, *pnb_elems, 19, c_lo_prefix_inf, c_lo_prefix_sup);
            myset(pauto, *pnb_elems, 21, c_hi_prefix_inf, c_hi_prefix_sup);
        } else {
            pauto[2].next_if_w_true = 3;
        }

        break;

    case RFMOD_TRIBIT_INVERTED:

// * AS WRITTEN EARLIER, NOT TESTED WITH A PREFIX *

        sz = (lo_prefix ? TRIBIT_INVERTED_NB_BYTES_WITH_PREFIX
                : TRIBIT_INVERTED_NB_BYTES_WITHOUT_PREFIX);
        *pnb_elems = (lo_prefix ? TRIBIT_INVERTED_NB_ELEMS_WITH_PREFIX
                : TRIBIT_INVERTED_NB_ELEMS_WITHOUT_PREFIX);

        pauto = (auto_t*)malloc(sz);
        my_pgm_memcpy(pauto, automat_tribit_inverted, sz);

        myset(pauto, *pnb_elems, 2, c_initseq, compact(65535));
        myset(pauto, *pnb_elems, 4, c_first_lo_ign_inf, c_first_lo_ign_sup);
        myset(pauto, *pnb_elems, 7, c_hi_short_inf, c_hi_short_sup);
        myset(pauto, *pnb_elems, 8, c_hi_long_inf, c_hi_long_sup);
        myset(pauto, *pnb_elems, 10, c_lo_long_inf, c_lo_long_sup);
        myset(pauto, *pnb_elems, 13, c_lo_short_inf, c_lo_short_sup);
        myset(pauto, *pnb_elems, 15, nb_bits, nb_bits);
        myset(pauto, *pnb_elems, 17, c_sep, compact(65535));

        if (lo_prefix) {
            myset(pauto, *pnb_elems, 19, c_lo_prefix_inf, c_lo_prefix_sup);
            myset(pauto, *pnb_elems, 21, c_hi_prefix_inf, c_hi_prefix_sup);
        } else {
            pauto[2].next_if_w_true = 3;
        }

        break;

    case RFMOD_MANCHESTER:

        assert(!lo_prefix);

        sz = MANCHESTER_NB_BYTES_WITHOUT_PREFIX;
        *pnb_elems = MANCHESTER_NB_ELEMS_WITHOUT_PREFIX;

        pauto = (auto_t*)malloc(sz);
        my_pgm_memcpy(pauto, automat_manchester, sz);

        myset(pauto, *pnb_elems, 2, c_initseq, compact(65535));
        myset(pauto, *pnb_elems, 4, c_lo_short_inf, c_lo_short_sup);
        myset(pauto, *pnb_elems, 6, c_hi_short_inf, c_hi_short_sup);
        myset(pauto, *pnb_elems, 9, c_lo_short_inf, c_lo_short_sup);
        myset(pauto, *pnb_elems, 11, c_hi_short_inf, c_hi_short_sup);
        myset(pauto, *pnb_elems, 12, c_hi_long_inf, c_hi_long_sup);
        myset(pauto, *pnb_elems, 14, nb_bits, nb_bits);
        myset(pauto, *pnb_elems, 16, nb_bits, nb_bits);
        myset(pauto, *pnb_elems, 18, c_lo_short_inf, c_lo_short_sup);
        myset(pauto, *pnb_elems, 19, c_lo_long_inf, c_lo_long_sup);
        myset(pauto, *pnb_elems, 21, nb_bits, nb_bits);
        myset(pauto, *pnb_elems, 23, c_hi_short_inf, c_hi_short_sup);
        myset(pauto, *pnb_elems, 25, c_lo_short_inf, c_lo_short_sup);
        myset(pauto, *pnb_elems, 26, c_lo_long_inf, c_lo_long_sup);
        myset(pauto, *pnb_elems, 28, nb_bits, nb_bits);
        myset(pauto, *pnb_elems, 29, nb_bits - 1, nb_bits - 1);
        myset(pauto, *pnb_elems, 30, c_sep, compact(65535));
        myset(pauto, *pnb_elems, 32, c_hi_long_inf, c_hi_long_sup);

        myset(pauto, *pnb_elems, 35, c_hi_short_inf, c_hi_long_sup);
        myset(pauto, *pnb_elems, 37, c_lo_short_inf, c_lo_long_sup);

        break;

    default:
        assert(false);
    }

        // Defensive programming
    for (byte i = 0; i < *pnb_elems; ++i) {
        assert(pauto[i].minval != 251);
        assert(pauto[i].maxval != 251);
    }

    return pauto;
}


// * ******** *****************************************************************
// * Receiver *****************************************************************
// * ******** *****************************************************************

Receiver::Receiver(auto_t *arg_dec, const unsigned short arg_dec_len,
            const byte arg_n):
        dec(arg_dec),
        dec_len(arg_dec_len),
        n(arg_n),
        status(0),
        has_value(false),
        callback_head(nullptr),
        next(nullptr) {

    recorded = new BitVector(n);

    assert(dec);
    assert(dec_len);
    assert(n);
    assert(recorded);
}

Receiver::~Receiver() {
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

    dbgf("Compare %u with [%u, %u]", val, minval, maxval);

    if (val < minval || val > maxval)
        return false;
    return true;
}

void Receiver::process_signal(duration_t compact_signal_duration,
        byte signal_val) {
    do {
        const auto_t *current = &dec[status];
        const byte w = current->w;

        bool r;
        switch (w) {
        case W_WAIT_SIGNAL:
            r = w_compare(current->minval, current->maxval, signal_val);
            break;

        case W_TERMINATE:
            has_value = true;
            r = true;
            break;

        case W_CHECK_DURATION:
            r = w_compare(current->minval, current->maxval,
                    compact_signal_duration);
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
        assert(next_status < dec_len);

        dbgf("d = %u, n = %d, status = %d, w = %d, next_status = %d",
                compact_signal_duration, recorded->get_nb_bits(), status, w,
                next_status);

        status = next_status;
    } while (dec[status].w != W_TERMINATE && dec[status].w != W_WAIT_SIGNAL);
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

void Receiver::execute_callbacks() {
    uint32_t t0 = millis();

    callback_t *pcb = callback_head;
    while (pcb) {

        if (!pcb->min_delay_between_two_calls
                || !pcb->last_trigger
                || t0 >= pcb->last_trigger + pcb->min_delay_between_two_calls) {

            if (!pcb->pcode || !pcb->pcode->cmp(recorded)) {
                pcb->last_trigger = t0;
                pcb->func(recorded);
            }
        }

        pcb = pcb->next;
    }
    reset();
}


// * ********** ***************************************************************
// * RF_manager ***************************************************************
// * ********** ***************************************************************

RF_manager::RF_manager(byte arg_pin_input_num, byte arg_int_num):
        int_num(arg_int_num),
        opt_wait_free_433(false),
        handle_int_receive_interrupts_is_set(false) {
    pin_input_num = arg_pin_input_num;
    head = nullptr;
    ++obj_count;

        // IT MAKES NO SENSE TO HAVE MORE THAN 1 RF_MANAGER
    assert(obj_count == 1);

}

RF_manager::~RF_manager() { }

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

    byte decoder_nb_elems;
    auto_t *decoder = build_automat(mod, initseq, lo_prefix, hi_prefix,
            first_lo_ign, lo_short, lo_long, hi_short, hi_long, lo_last, sep,
            nb_bits, &decoder_nb_elems);

    Receiver *ptr_rec = new Receiver(decoder, decoder_nb_elems, nb_bits);
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

    Receiver* ptr_rec = head;
    while (ptr_rec) {
        if (ptr_rec->get_has_value()) {

            if (opt_wait_free_433) {
                if (!has_waited_free_433) {
                    wait_free_433();
                    has_waited_free_433 = true;
                }
            }

            ptr_rec->execute_callbacks();

        }
        ptr_rec = ptr_rec->get_next();
    }

        // Why reset everything when wait_free_433 is executed?
        // Because the timings are then completely messed up, and the ongoing
        // recording already done by any open receiver must be restarted from
        // scratch.
    if (has_waited_free_433) {

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

    attachInterrupt(int_num, &ih_handle_interrupt_wait_free, CHANGE);

        // 75% of the last 16 durations must be in the interval [200, 25000]
        // (that is, 12 out of 16).
    while (IH_wait_free_count_ok >= 12)
        ;

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

#ifdef SIMULATE_INTERRUPTS
const uint16_t timings[] PROGMEM = {
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

    0, 0
};
const size_t timings_len = sizeof(timings) / sizeof(*timings);
size_t timings_index = 0;
bool has_read_all_timings() { return timings_index >= timings_len; }
#endif

bool handle_int_overrun = false;
bool handle_int_busy = false;

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
    dbg("--");
#endif

    if (handle_int_busy) {
        handle_int_overrun = true;
        return;
    }

    if (handle_int_overrun) {
        signal_duration = 0;
        handle_int_overrun = false;
    }
    handle_int_busy = true;
    sei();

#ifdef SIMULATE_INTERRUPTS
            byte signal_val = !(timings_index % 2);
#else
            byte signal_val =
                (digitalRead(RF_manager::get_pin_input_num()) == HIGH ? 1 : 0);
#endif

    duration_t compact_signal_duration = compact(signal_duration);
    Receiver *ptr_rec = RF_manager::get_head();
    while (ptr_rec) {
        dbgf("\nptr_rec = %lu", (unsigned long)ptr_rec);
        ptr_rec->process_signal(compact_signal_duration, signal_val);
        ptr_rec = ptr_rec->get_next();
    }

    handle_int_busy = false;
}

// vim: ts=4:sw=4:tw=80:et
