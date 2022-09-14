ALL: 02_recv.ino.with_bootloader.hex

color: 02_recv.ino
	./am2 02_recv.ino

02_recv.ino.with_bootloader.hex: 02_recv.ino
	GCC_COLORS="" ./am2 --no-color 02_recv.ino

clean:
	rm -rf build

mrproper:
	rm -rf build out
