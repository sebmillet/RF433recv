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

//#define SIMULATE_INTERRUPTS

#define ASSERT_OUTPUT_TO_SERIAL

#define assert(cond) { \
    if (!(cond)) { \
        rf433recv_assert_failed(__LINE__); \
    } \
}

static void rf433recv_assert_failed(int line) {
#ifdef ASSERT_OUTPUT_TO_SERIAL
    Serial.print("\nRF433recv.cpp:");
    Serial.print(line);
    Serial.println(": assertion failed, aborted.");
#endif
    while (1)
        ;
}

#define delayed_assert(cond) { \
    if (!(cond)) { \
        delayed_assert = __LINE__; \
    } \
}

unsigned int delayed_assert = 0;
void check_delayed_assert() {
    if (!delayed_assert)
        return;
    Serial.print("delayed assert failed line ");
    Serial.print(delayed_assert);
    Serial.print("\n");
    while (1)
        ;
}

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
#ifndef COMPACT_DURATIONS
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
#ifndef COMPACT_DURATIONS
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
    if (val < minval || val > maxval)
        return false;
    return true;
}

void Receiver::process_signal(duration_t signal_duration, byte signal_val) {
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
                    compact(signal_duration));
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

        dbgf("d = %u, n = %d, status = %d, w = %d, next_status = %d",
                signal_duration, recorded->get_nb_bits(), status, w,
                next_status);

        status = next_status;
    } while (dec[status].w != W_TERMINATE && dec[status].w != W_WAIT_SIGNAL);
}

void Receiver::attach(Receiver* ptr_rec) {
    assert(!next);
    next = ptr_rec;
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

void RF_manager::register_Receiver(auto_t *arg_dec,
        const unsigned short arg_dec_len, const byte arg_n) {
    Receiver *ptr_rec = new Receiver(arg_dec, arg_dec_len, arg_n);
    Receiver *tail = get_tail();
    if (!tail) {
        head = ptr_rec;
    } else {
        tail->attach(ptr_rec);
    }
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
        check_delayed_assert();
    }
    inactivate_interrupts_handler();
}

byte RF_manager::pin_input_num = 255;
Receiver* RF_manager::head = nullptr;
byte RF_manager::obj_count = 0;


// * ***************** ********************************************************
// * Interrupt Handler ********************************************************
// * ***************** ********************************************************

#ifdef SIMULATE_INTERRUPTS
uint16_t timings[] = {
  0   ,  7064,
  1160,   632,
   456,  1344,
  1156,   656,
  1156,   660,
   448,  1376,
  1148,   684,
   456,  1388,
  1148,   652,
   456,  1336,
   452,  1340,
  1152,   652,
  1148,   672,
   456,  1372,
  1156,   692,
   436,  1412,
  1124,   676,
   436,  1348,
  1132,   656,
  1148,   660,
   448,  1372,
  1148,   680,
  1140,   688,
   440,  1404,
  1140,   664,
   436,  1364,
   424,  1376,
   412,  1392,
   412,  1396,
   416,  1404,
   416,  1412,
   420,  1424,
   420,  1348,
   416,  7136,
  1072,   720,
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

    Receiver *ptr_rec = RF_manager::get_head();
    while (ptr_rec) {
        ptr_rec->process_signal(signal_duration, signal_val);
        ptr_rec = ptr_rec->get_next();
    }

    handle_int_busy = false;
}

// vim: ts=4:sw=4:tw=80:et
