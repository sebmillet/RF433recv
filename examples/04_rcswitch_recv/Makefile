ALL: 04_rcswitch_recv.ino.with_bootloader.hex

color: 04_rcswitch_recv.ino
	./am2 04_rcswitch_recv.ino

04_rcswitch_recv.ino.with_bootloader.hex: 04_rcswitch_recv.ino
	GCC_COLORS="" ./am2 --no-color 04_rcswitch_recv.ino

clean:
	rm -rf build

mrproper:
	rm -rf build out
