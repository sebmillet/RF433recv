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

    // The below one corresponds to RFMOD_TRIBIT
const auto_t automat_tribit[] PROGMEM = {

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

// IMPORTANT - FIXME FIXME FIXME
// ***NOT TESTED WITH A PREFIX***
// IN REAL CONDITIONS, TESTED ONLY *WITHOUT* PREFIX
const auto_t automat_tribit_inverted[] PROGMEM = {

// Below, (T) means 'next status if test returns true' and
//        (F) means 'next status if test returns false'.

//    WHAT TO DO      MINVAL MAXVAL (T)  (F)
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

const auto_t automat_manchester[] PROGMEM = {

// Below, (T) means 'next status if test returns true' and
//        (F) means 'next status if test returns false'.

//    WHAT TO DO      MINVAL MAXVAL (T)  (F)
    { W_WAIT_SIGNAL,        1,     1,  2,   0 }, //  0
    { W_TERMINATE,          0,     0,  1,  99 }, //  1
    { W_CHECK_DURATION,  4000, 65535,  3,   0 }, //  2

    { W_WAIT_SIGNAL,        0,     0,  4,   0 }, //  3
    { W_CHECK_DURATION,   700,  1400,  5,   0 }, //  4
    { W_WAIT_SIGNAL,        1,     1,  6,   0 }, //  5
    { W_CHECK_DURATION,   700,  1400,  7,   2 }, //  6

    { W_RESET_BITS,         0,     0,  8,  99 }, //  7

    { W_WAIT_SIGNAL,        0,     0,  9,   0 }, //  8
    { W_CHECK_DURATION,   700,  1600, 10,   0 }, //  9

    { W_WAIT_SIGNAL,        1,     1, 11,   0 }, // 10
    { W_CHECK_DURATION,   700,  1700, 13,  12 }, // 11
    { W_CHECK_DURATION,  1700,  2800, 15,   2 }, // 12

    { W_ADD_ZERO,           0,     0, 14,   2 }, // 13
    { W_CHECK_BITS,        32,    32,  1,   8 }, // 14

    { W_ADD_ZERO,           0,     0, 16,   2 }, // 15
    { W_CHECK_BITS,        32,    32,  1,  17 }, // 16
    { W_WAIT_SIGNAL,        0,     0, 18,   0 }, // 17
    { W_CHECK_DURATION,   700,  1600, 20,  19 }, // 18
    { W_CHECK_DURATION,  1700,  2800, 27,   0 }, // 19

    { W_ADD_ONE,            1,     1, 21,   0 }, // 20
    { W_CHECK_BITS,        32,    32,  1,  22 }, // 21
    { W_WAIT_SIGNAL,        1,     1, 23,   0 }, // 22
    { W_CHECK_DURATION,   700,  1600, 24,   2 }, // 23
    { W_WAIT_SIGNAL,        0,     0, 25,   0 }, // 24
    { W_CHECK_DURATION,   700,  1600, 20,  26 }, // 25
    { W_CHECK_DURATION,  1700,  2800, 27,   0 }, // 26

    { W_ADD_ONE,            0,     0, 28,   0 }, // 27
    { W_CHECK_BITS,        32,    32,  1,  10 }, // 28
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
    const char *walker = (const char*)src;
    char *d = (char *)dest;
    for (size_t i = 0; i < n; ++i) {
        *(d++) = pgm_read_byte(walker++);
    }
}

auto_t* build_automat(byte mod, uint16_t initseq, uint16_t lo_prefix,
        uint16_t hi_prefix, uint16_t first_lo_ign, uint16_t lo_short,
        uint16_t lo_long, uint16_t hi_short, uint16_t hi_long, uint16_t lo_last,
        uint16_t sep, byte nb_bits, byte *pnb_elems) {

    assert((lo_prefix && hi_prefix) || (!lo_prefix && !hi_prefix));
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

        break;

    default:
        assert(false);
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

            pcb->last_trigger = t0;
            pcb->func(recorded);
        }

        pcb = pcb->next;
    }
    reset();
}


// * ********** ***************************************************************
// * RF_manager ***************************************************************
// * ********** ***************************************************************

RF_manager::RF_manager(byte arg_pin_input_num, byte arg_int_num):
        int_num(arg_int_num) {
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
#ifndef SIMULATE_INTERRUPTS
    attachInterrupt(int_num, &handle_int_receive, CHANGE);
#endif
}

void RF_manager::inactivate_interrupts_handler() {
#ifndef SIMULATE_INTERRUPTS
    detachInterrupt(int_num);
#endif
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
    Receiver* ptr_rec = head;
    while (ptr_rec) {
        if (ptr_rec->get_has_value()) {
            ptr_rec->execute_callbacks();
            return;
        }
        ptr_rec = ptr_rec->get_next();
    }
}

void RF_manager::register_callback(void (*func)(const BitVector *recorded),
        uint32_t min_delay_between_two_calls) {

    Receiver *tail = get_tail();
    assert(tail); // A bit brutal... caller should know that register_callback
                  // can be done only after registering a Receiver.

    callback_t *pcb = new callback_t;
    pcb->pcode = nullptr;
    pcb->func = func;
    pcb->min_delay_between_two_calls = min_delay_between_two_calls;
    pcb->last_trigger = 0;
    pcb->next = nullptr;

    tail->add_callback(pcb);
}

byte RF_manager::pin_input_num = 255;
Receiver* RF_manager::head = nullptr;
byte RF_manager::obj_count = 0;


// * ***************** ********************************************************
// * Interrupt Handler ********************************************************
// * ***************** ********************************************************

#ifdef SIMULATE_INTERRUPTS
uint16_t timings[] = {
    0,  5348,
    1132,  1320,
    1156,  2560,
    2392,  1328,
    1088,  1376,
    1088,  1340,
    1136,  1320,
    1200,  1280,
    1176,  1280,
    1196,  1248,
    1108,  1328,
    1188,  1316,
    1080,  1368,
    1100,  1348,
    1144,  2520,
    1260,  1228,
    1204,  1244,
    2336,  1404,
    1144,  1292,
    1156,  2544,
    2480,  1232,
    1092,  2592,
    2344,  2584,
    2372,  1368,
    1108,  2548,
    1176,  1304,
    1204,  1236,
    2368,  5360,
    1204,  1244
};
const size_t timings_len = sizeof(timings) / sizeof(*timings);
byte timings_index = 0;
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
        signal_duration = timings[timings_index];
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
        ptr_rec->process_signal(compact_signal_duration, signal_val);
        ptr_rec = ptr_rec->get_next();
    }

    handle_int_busy = false;
}

// vim: ts=4:sw=4:tw=80:et
