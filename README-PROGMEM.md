About decoding automats being stores in PROGMEM
-----------------------------------------------

1. The issue

Inside RF433recv, the signal decoding is done on-the-fly: there is no storage
of durations followed by analysis. This simplifies a few things (like, the
starting threshold of signal recording: there is no such starting threshold),
but implies the decoding to be as fast as possible while inside the interrupt
handler.

The decoding automat structures are big and putting them in the regular RAM is
not ideal, considering the RAM size available.

See in the file RF433recv.cpp, the arrays:

    automat_tribit
    automat_tribit_inverted
    automat_manchester

It is therefore best to save the automats in PROGMEM, to the cost of reading
them partially from the PROGMEM as we need it (we never read it entirely of
course, we read only the values that we need to step forward in the automat).
This has a performance impact that the author wanted to have an idea of.

1. Benefit

Putting the automats in the PROGMEM saves 415 bytes of memory.

See macro OUTPUT_SIZEOF_AUTOMATS_AT_COMPILE_TIME in `RF433recv.cpp`.

2. Cost

The performance impact has been estimated by measuring the interrupt handler
execution duration. See in the code the macro DEBUG_EXEC_TIMES.

12 decoders were registered, those were the 12 RCSwitch protocols as of
2022-01-08.

See [examples/04_rcswitch_recv/04_rcswitch_recv.ino](examples/04_rcswitch_recv/04_rcswitch_recv.ino).

The raw figures are stored in the files:

    (1) examples/04_rcswitch_recv/04_rcswitch_recv-ref.txt
    (2) examples/04_rcswitch_recv/04_rcswitch_recv-progmem.txt

(1) is the measure without PROGMEM, (2) with PROGMEM.

The calculations are saved in the below libreoffice calc document:

    examples/04_rcswitch_recv/04_rcswitch_recv-calc.ods

Below, I indicate the std dev just for the matter of controlling the values are
not too scattered.

- Execution times WITHOUT PROGMEM

Average of minimum execution times: 178 (std dev: 28)
Average of average execution times: 250 (std dev: 21)
Average of maximum execution times: 328 (std dev: 31)

- Execution times WITH PROGMEM

Average of minimum execution times: 166 (std dev: 18)
Average of average execution times: 241 (std dev: 20)
Average of maximum execution times: 321 (std dev: 38)

3. Difference

Going from no-PROGMEM to PROGMEM, the avg-minimum reduced by 7%, the
avg-average reduced by 4%, the avg-maximum reduced by 2%.

This is rather unexpected. The use of PROGMEM reduced execution time??!!!

The author's explanation: it could be that reading PROGMEM involves some kind
of interrupt handling (interrupts being blocked during execution of
pgm_read_\*), causing the time measure to drift a bit.

This is rather worrying as it coult be that real execution time becomes really
bigger! At this stage the author has no way to ascertain it and calculate more
precise execution times, presumably it'd require additional hardware to make
precise measures against, taken from outside of the controller.

4. Other tests done

After the implementation of PROGMEM-storage of automats, saving more than 400
bytes of memory, the author did various tests with real-life telecommands, and
could not find a difference in the quality of reception.


Conclusion
----------

While the real impact of PROGMEM usage remains to be calculated, it is small
enough that RF433recv performance works in the same situations as it did before
PROGMEM usage.

