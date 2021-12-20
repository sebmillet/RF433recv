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

#ifndef _RF433RECV_H
#define _RF433RECV_H

    // Don't uncomment the below unless you know what you are doing
//#define RF433RECV_TESTPLAN 1

// ****************************************************************************
// RF433RECV_TESTPLAN *********************************************************
#if RF433RECV_TESTPLAN == 1

//#define DEBUG
#define SIMULATE_INTERRUPTS

#else // RF433RECV_TESTPLAN

#ifdef RF433RECV_TESTPLAN
#error "RF433RECV_TESTPLAN macro has an illegal value."
#endif
// RF433RECV_TESTPLAN *********************************************************
// ****************************************************************************

// It is OK to update the below, because if this code is compiled, then we are
// not in the test plan.

//#define DEBUG
//#define SIMULATE_INTERRUPTS

#endif // RF433ANY_TESTPLAN

    // The possibility *not* to compact durations is available for debugging
    // purposes. You should not use it in normal circumstances.
//#define NO_COMPACT_DURATIONS

#ifdef NO_COMPACT_DURATIONS
#pragma message ("NO_COMPACT_DURATIONS MACRO DEFINED!")
#warning "NO_COMPACT_DURATIONS MACRO DEFINED!"
#endif

    // Don't uncomment this unless you know what you are doing
//#define NO_PROGMEM_FOR_AUTOMAT_TEMPLATES

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
        byte target_nb_bits;
        byte target_nb_bytes;
        byte nb_bits;
    public:
        BitVector(byte arg_target_nb_bits);
        ~BitVector();

        BitVector(short arg_nb_bits, short arg_nb_bytes, byte b0, byte b1);
        BitVector(short arg_nb_bits, short arg_nb_bytes, byte b0, byte b1,
                byte b2);
        BitVector(short arg_nb_bits, short arg_nb_bytes, byte b0, byte b1,
                byte b2, byte b3);
        BitVector(short arg_nb_bits, short arg_nb_bytes, byte b0, byte b1,
                byte b2, byte b3, byte b4);
        BitVector(short arg_nb_bits, short arg_nb_bytes, byte b0, byte b1,
                byte b2, byte b3, byte b4, byte b5);

        void prepare_BitVector_construction(short arg_nb_bits,
                short arg_nb_bytes, short n);

        void reset();

        void add_bit(byte v);

        int get_nb_bits() const;
        byte get_nb_bytes() const;
        byte get_nth_bit(byte n) const;
        byte get_nth_byte(byte n) const;

        char *to_str() const;

        short cmp(const BitVector *p) const;
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
#ifdef NO_COMPACT_DURATIONS
#define duration_t uint16_t
#else
#define duration_t byte
#endif

duration_t compact(uint16_t u);
uint16_t uncompact(duration_t b);

#define RFMOD_TRIBIT          0
#define RFMOD_TRIBIT_INVERTED 1
#define RFMOD_MANCHESTER      2

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

struct callback_t {
    const BitVector *pcode;
    void (*func)(const BitVector *recorded);
    uint32_t min_delay_between_two_calls;
    uint32_t last_trigger;

    callback_t *next;
};

auto_t* build_automat(byte mod, uint16_t initseq, uint16_t lo_prefix,
        uint16_t hi_prefix, uint16_t first_lo_ign, uint16_t lo_short,
        uint16_t lo_long, uint16_t hi_short, uint16_t hi_long, uint16_t lo_last,
        uint16_t sep, byte nb_bits, byte *pnb_elems);

class Receiver {
    private:
        auto_t *dec;
        const unsigned short dec_len;
        const byte n;
        byte status;
        BitVector *recorded;
        bool has_value;

        callback_t *callback_head;

        Receiver *next;

        bool w_compare(duration_t minval, duration_t maxval, duration_t val)
            const;

        callback_t* get_callback_tail() const;

    public:
        Receiver(auto_t *arg_dec, const unsigned short arg_dec_len,
                const byte n);
        ~Receiver();

        void process_signal(duration_t compact_signal_duration,
                byte signal_val);

        void reset();

        bool get_has_value() const { return has_value; }
        const BitVector *get_recorded() const { return recorded; }

        Receiver* get_next() const { return next; }
        void attach(Receiver* ptr_rec);

        void add_callback(callback_t *pcb);
        void execute_callbacks();
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

        static volatile uint16_t IH_wait_free_last16;
        static volatile short IH_wait_free_count_ok;

        byte int_num;

        bool opt_wait_free_433;
        bool handle_int_receive_interrupts_is_set;

    public:

        static byte get_pin_input_num() { return pin_input_num; }
        static Receiver* get_head() { return head; }
        static Receiver* get_tail();

        static void ih_handle_interrupt_wait_free();

        RF_manager(byte arg_pin_input_num, byte arg_int_num);
        ~RF_manager();

        void register_Receiver(byte mod, uint16_t initseq, uint16_t lo_prefix,
                uint16_t hi_prefix, uint16_t first_lo_ign, uint16_t lo_short,
                uint16_t lo_long, uint16_t hi_short, uint16_t hi_long,
                uint16_t lo_last, uint16_t sep, byte nb_bits,
                void (*func)(const BitVector *recorded) = nullptr,
                uint32_t min_delay_between_two_calls = 0);

        bool get_has_value() const;
        Receiver* get_receiver_that_has_a_value() const;

        void activate_interrupts_handler();
        void inactivate_interrupts_handler();

        void wait_value_available();

        void register_callback(void (*func) (const BitVector *recorded),
                uint32_t min_delay_between_two_calls,
                const BitVector *pcode = nullptr);

        void do_events();

        void set_opt_wait_free_433(bool v) { opt_wait_free_433 = v; }
        void wait_free_433();
};

#endif // _RF433RECV_H

// vim: ts=4:sw=4:tw=80:et
