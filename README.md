RF433recv
=========

Uses a RF433Mhz component plugged on an Arduino to listen to known signals and
decode it.


Installation
------------

Download a zip of this repository, then include it from the Arduino IDE.


Schematic
---------

1. Arduino board. Tested with NANO and UNO.

2. Radio Frequence 433Mhz RECEIVER like MX-RM-5V.

RF433 RECEIVER data pin must be plugged on a board' digital PIN that can
trigger interrupts, that is, D2 or D3.


Usage
-----

To see how to call library, you'll find an example here:

[examples/01_generic/01_generic.ino](examples/01_generic/01_generic.ino)


How to work out signal characteristics using the library RF433any
-----------------------------------------------------------------

The function `register_Receiver` takes signal characteristics (encoding type,
signal timings) to do its job. How to work it out?

The painful way is to record signal, using for example a low level signal
recording like gnuradio or a Radio Frequencies sniffing library/code like
[https://github.com/sebmillet/rf433snif](https://github.com/sebmillet/rf433snif)
(There are many others.)
Then, to deduct signal timings. Good luck!

An easier way is to use RF433any library, found here:

[https://github.com/sebmillet/RF433any](https://github.com/sebmillet/RF433any)

and to execute the code 01_main.ino found in the folder
examples/01_main

Ultimately the code is the below one:

[https://github.com/sebmillet/RF433any/blob/main/examples/01_main/01_main.ino](https://github.com/sebmillet/RF433any/blob/main/examples/01_main/01_main.ino)

This code will output what `register_Receiver` needs to be called with, so as
to decode the signal received.

For example, if `01_main.ino` outputs the below:

    Waiting for signal
    Data: 8a 34 e6 bf

    -----CODE START-----
    // [WRITE THE DEVICE NAME HERE]
    rf.register_Receiver(
        RFMOD_TRIBIT, // mod
        7056, // initseq
        0, // lo_prefix
        0, // hi_prefix
        0, // first_lo_ign
        580, // lo_short
        1274, // lo_long
        0, // hi_short (0 => take lo_short)
        0, // hi_long  (0 => take lo_long)
        520, // lo_last
        7056, // sep
        32  // nb_bits
    );
    -----CODE END-----

Then you can manage reception like this:

```c++
#include "RF433recv.h"
#include <Arduino.h>

    // Depends on your schematic - here, the Radio Frequencies device is
    // plugged on Arduino' D2, that corresponds to interrupt number 0.
#define PIN_RFINPUT  2
#define INT_RFINPUT  0

RF_manager rf(PIN_RFINPUT, INT_RFINPUT);

void callback1(const BitVector *recorded) {
    Serial.print(F("Signal received from telecommand\n"));
}

void callback2(const BitVector *recorded) {
    Serial.print(F("Signal received from telecommand, code = 8A34E6BF\n"));
}

void setup() {
    pinMode(PIN_RFINPUT, INPUT);
    Serial.begin(115200);

        // [OTIO (no rolling code, 32-bit)]
    rf.register_Receiver(
        RFMOD_TRIBIT, // mod
        4956,         // initseq
        0,            // lo_prefix
        0,            // hi_prefix
        0,            // first_lo_ign
        580,          // lo_short
        1274,         // lo_long
        0,            // hi_short (0 => take lo_short)
        0,            // hi_long  (0 => take lo_long)
        520,          // lo_last
        4956,         // sep
        32,           // nb_bits
        callback1,
        2000
    );
    rf.register_callback(callback2, 2000,
            new BitVector(32, 4, 0x8A, 0x34, 0xE6, 0xBF));

    Serial.print(F("Waiting for signal\n"));

    rf.activate_interrupts_handler();
}

void loop() {
    rf.do_events();
}
```

You can call register_callback without specifying the code, too. The code above
is equivalent to the below (only showing what changes):

```c++
// ...
        // [OTIO (no rolling code, 32-bit)]
    rf.register_Receiver(
        RFMOD_TRIBIT, // mod
        4956,         // initseq
        0,            // lo_prefix
        0,            // hi_prefix
        0,            // first_lo_ign
        580,          // lo_short
        1274,         // lo_long
        0,            // hi_short (0 => take lo_short)
        0,            // hi_long  (0 => take lo_long)
        520,          // lo_last
        4956,         // sep
        32            // nb_bits
    );
    rf.register_callback(callback1, 2000);
    rf.register_callback(callback2, 2000,
            new BitVector(32, 4, 0x8A, 0x34, 0xE6, 0xBF));
// ...
```

The value 2000 is the minimum the delay in milliseconds between two calls, so
here it makes 2000 milliseconds = 2 seconds.


Link with RCSwitch library
--------------------------

RSwitch library is available in Arduino library manager. It is also available
on github, here:
[https://github.com/sui77/rc-switch/](https://github.com/sui77/rc-switch/)

The example
[examples/04_rcswitch_recv/04_rcswitch_recv.ino](examples/04_rcswitch_recv/04_rcswitch_recv.ino)
shows how to decode RCSwitch protocols.


About decoding automats being stored in PROGMEM
-----------------------------------------------

This has a performance impact that was measured, see file
[README-PROGMEM.md](README-PROGMEM.md).


About decoding multiple protocols in parallel
---------------------------------------------

You can register as many decoders as you wish (calling register_Receiver). But
since the decoding takes place in the interrupt handler, if too many decoders
are registered, at some point the signals with short typical durations won't
work.

Just as an example:

- Inside
[examples/04_rcswitch_recv/04_rcswitch_recv.ino](examples/04_rcswitch_recv/04_rcswitch_recv.ino),
if calling register_Receiver for each of the 12 RCSwitch protocols, then
all protocols work fine except: 1, 7, 11 and 12.

- Inside
[examples/04_rcswitch_recv/04_rcswitch_recv.ino](examples/04_rcswitch_recv/04_rcswitch_recv.ino)
still, if registering only the protocols 1, 2, 3, 4, 5, 6, 8, 9, 10, then
each of these protocols works fine.

- The most difficult RCSwitch protocol in this regard is 7: it'll work along
with 2 other decoders, but not 3.

