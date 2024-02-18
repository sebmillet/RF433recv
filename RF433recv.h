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

#endif // RF433RECV_TESTPLAN

    // The possibility *not* to compact durations is available for debugging
    // purposes. You should not use it in normal circumstances.
//#define NO_COMPACT_DURATIONS

#ifdef NO_COMPACT_DURATIONS
#pragma message ("NO_COMPACT_DURATIONS MACRO DEFINED!")
#warning "NO_COMPACT_DURATIONS MACRO DEFINED!"
#endif

//#define DEBUG_AUTOMAT
#if defined(DEBUG_AUTOMAT) && !defined(DEBUG)
#error "DEBUG_AUTOMAT macro set without DEBUG"
#endif

//#define DEBUG_EXEC_TIMES

    // *IMPORTANT*
    //   MUST BE A POWER OF 2
    //     Because we need to calculate "modulo BUFFER_SIGNALS_NB", and as we
    //     don't want a long calculation to take place (yes we are in a
    //     microcontroller, plus, we are INSIDE an interrupt call...), we simply
    //     use "& BUFFER_SIGNALS_MASK" instruction.
    //   ALSO MAX VALUE IS 128 (BECAUSE THE VARIABLES TO WALK THROUGH THE BUFFER
    //   ARE OF TYPE byte)
    //   Note that we use a read_head and write_head, and both being equal
    //   means, there is no value to read. As a result, the real buffer size is
    //   not BUFFER_SIGNALS_NB but (BUFFER_SIGNALS_NB - 1).
    //   So with a 'raw' buffer size of 4, we have a real buffer of 3 durations
    //   waiting for us to process. This is sufficient for our need.
#define BUFFER_SIGNALS_NB 4 // DON'T CHANGE IT UNLESS YOU KNOW WHAT YOU ARE
                            // DOING!!!
#define BUFFER_SIGNALS_MASK (BUFFER_SIGNALS_NB - 1)

#ifdef DEBUG

#include "RF433Debug.h"
#include "RF433MemoryFree.h"

#else

#define dbg(a)
#define dbgf(...)

#endif

#include <Arduino.h>


// * **************** *********************************************************
// * MeasureExecTimes *********************************************************
// * **************** *********************************************************

#ifdef DEBUG_EXEC_TIMES

class MeasureExecTimes {
    private:
        unsigned long int dmin;
        unsigned long int dmax;
        unsigned long int dtotal;
        unsigned long int count;
        unsigned long int reset_every;

    public:
        MeasureExecTimes(unsigned long int arg_reset_every);
        MeasureExecTimes();
        ~MeasureExecTimes();

        void add(unsigned long int d);
        void output_stats(const char *name) const;

        void reset();
};

void output_measureexectimes_stats();

#endif


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

enum ad_field_idx {
    AD_INITSEQ_INF,
    AD_LO_PREFIX_INF,
    AD_LO_PREFIX_SUP,
    AD_HI_PREFIX_INF,
    AD_HI_PREFIX_SUP,
    AD_FIRST_LO_IGN_INF,
    AD_FIRST_LO_IGN_SUP,
    AD_LO_SHORT_INF,
    AD_LO_SHORT_SUP,
    AD_LO_LONG_INF,
    AD_LO_LONG_SUP,
    AD_HI_SHORT_INF,
    AD_HI_SHORT_SUP,
    AD_HI_LONG_INF,
    AD_HI_LONG_SUP,
    AD_LO_LAST_INF,
    AD_LO_LAST_SUP,
    AD_SEP_INF,
    AD_NB_BITS,
    AD_NEXT_PREFIX,
    AD_NEXT_SPECIAL,
    AD_BIT_0,
    AD_BIT_1,
    AD_NB_FIELDS,
        // The indexes named 'ADX_' (instead of 'AD_') are *not* used to
        // designate an element inside autoexec_t line:
        //   duration_t values[AD_NB_FIELDS];
        // Why 196?
        //   Because it is big enough to show to this code reader that it is
        //   different from AD_ constants.
        //   I could have chosen 50, or 128, or 200.
    ADX_UNDEF = 196,    // Keep in mind, 196 is 100% meaningless - only
                        // important thing is, it must be far enough from 255
                        // (so that other ADX_ constants have enough values
                        // left) and above to, or equal to, AD_NB_FIELDS.
    ADX_DMAX,
    ADX_ZERO,
    ADX_ONE,
    ADX_NB_BITS_M1
};
#define AD_INDIRECT 0x80

struct autoline_t {
    byte w;
    byte ad_field_idx_minval;
    byte ad_field_idx_maxval;
    byte next_if_w_true;
    byte next_if_w_false;
};

struct autoexec_t {
    const autoline_t *mat;
    unsigned short mat_len;
    duration_t values[AD_NB_FIELDS];
};

struct callback_t {
    const BitVector *pcode;
    void (*func)(const BitVector *recorded);
    uint32_t min_delay_between_two_calls;
    uint32_t last_trigger;

    callback_t *next;
};

autoexec_t* build_automat(byte mod, uint16_t initseq, uint16_t lo_prefix,
        uint16_t hi_prefix, uint16_t first_lo_ign, uint16_t lo_short,
        uint16_t lo_long, uint16_t hi_short, uint16_t hi_long, uint16_t lo_last,
        uint16_t sep, byte nb_bits);

class Receiver {
    private:
        const autoexec_t *pax;
        const byte n;
        byte status;
        BitVector *recorded;
        bool has_value;

        callback_t *callback_head;

        Receiver *next;

        bool w_compare(duration_t minval, duration_t maxval, duration_t val)
            const;

        callback_t* get_callback_tail() const;

        duration_t get_val(byte idx) const;

    public:
        Receiver(autoexec_t *arg_pax, byte n);
        ~Receiver();

        void process_signal(duration_t compact_signal_duration,
                byte signal_val);

        void reset();

        bool get_has_value() const { return has_value; }
        const BitVector *get_recorded() const { return recorded; }

        Receiver* get_next() const { return next; }
        void attach(Receiver* ptr_rec);

        void add_callback(callback_t *pcb);
        byte execute_callbacks();
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

        bool opt_wait_free_433_is_set;
        uint32_t opt_wait_free_433_timeout;

        bool handle_int_receive_interrupts_is_set;

        bool first_decoder_that_has_a_value_resets_others;
        bool inactivate_interrupts_handler_when_a_value_has_been_received;

    public:

        static byte get_pin_input_num() { return pin_input_num; }
        static Receiver* get_head() { return head; }
        static Receiver* get_tail();

        static void ih_handle_interrupt_wait_free();

        void activate_interrupts_handler();
        void inactivate_interrupts_handler();

        RF_manager(byte arg_pin_input_num);
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

        void wait_value_available();

        void register_callback(void (*func) (const BitVector *recorded),
                uint32_t min_delay_between_two_calls,
                const BitVector *pcode = nullptr);

        void do_events();

        void set_opt_wait_free_433(bool v, uint32_t timeout = 0);
        void wait_free_433();

        void set_first_decoder_that_has_a_value_resets_others(bool val) {
            first_decoder_that_has_a_value_resets_others = val;
        }
        void set_inactivate_interrupts_handler_when_a_value_has_been_received(
                bool val) {
            inactivate_interrupts_handler_when_a_value_has_been_received = val;
        }
};

#ifdef SIMULATE_INTERRUPTS
bool has_read_all_timings();
#endif

#endif // _RF433RECV_H

// vim: ts=4:sw=4:tw=80:et
