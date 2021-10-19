// RF433recv.h

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

//#define DEBUG

#ifndef _RF433RECV_H
#define _RF433RECV_H

#ifdef DEBUG

#include "RF433Debug.h"

#else

#define dbg(a)
#define dbgf(...)

#endif

#include <Arduino.h>

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


// * ******** *****************************************************************
// * Receiver *****************************************************************
// * ******** *****************************************************************

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

duration_t compact(uint16_t u);
uint16_t uncompact(duration_t b);

#define W_WAIT_SIGNAL    0
#define W_TERMINATE      1
#define W_CHECK_DURATION 2
#define W_RESET_BITS     3
#define W_ADD_ZERO       4
#define W_ADD_ONE        5
#define W_CHECK_BITS     6

struct auto_t {
    byte w;
    duration_t minval;
    duration_t maxval;
    byte next_if_w_true;
    byte next_if_w_false;
};

class Receiver {
    private:
        auto_t *dec;
        const unsigned short dec_len;
        const byte n;
        byte status;
        BitVector *recorded;
        bool has_value;
        Receiver *next;

        bool w_compare(duration_t minval, duration_t maxval, duration_t val)
            const;

    public:
        Receiver(auto_t *arg_dec, const unsigned short arg_dec_len,
                const byte n);
        ~Receiver();

        void process_signal(duration_t signal_duration, byte signal_val);

        void reset();

        bool get_has_value() const { return has_value; }
        const BitVector *get_recorded() const { return recorded; }

        Receiver* get_next() const { return next; }
        void attach(Receiver* ptr_rec);
};


// * ********** ***************************************************************
// * RF_manager ***************************************************************
// * ********** ***************************************************************

// IMPORTANT
//   KEEP IN MIND RF_MANAGER CAN BE INSTANCIATED ONLY ONCE.
// It is verified with obj_count static property (if it goes above 1, it causes
// an assert to fail).
//
// This means, the distinction between static and non-static properties and
// static and non static member functions is pointless.
// It is done though, for the sake of... I don't know what!
class RF_manager {
    private:

        static byte obj_count; // Should never be > 1
        static byte pin_input_num;
        static Receiver *head;

        byte int_num;

    public:

        static byte get_pin_input_num() { return pin_input_num; }
        static Receiver* get_head() { return head; }
        static Receiver* get_tail();

        RF_manager(byte arg_pin_input_num, byte arg_int_num);
        ~RF_manager();

        void register_Receiver(auto_t *arg_dec,
                const unsigned short arg_dec_len, const byte arg_n);
        bool get_has_value() const;
        Receiver* get_receiver_that_has_a_value() const;

        void activate_interrupts_handler();
        void inactivate_interrupts_handler();

        void wait_value_available();
};

#endif // _RF433RECV_H

// vim: ts=4:sw=4:tw=80:et
