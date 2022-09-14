ALL: test.ino.with_bootloader.hex

color: test.ino
	./am2 -t 1 test.ino

test.ino.with_bootloader.hex: test.ino
	GCC_COLORS="" ./am2 --no-color -t 1 test.ino

clean:
	rm -rf build

mrproper:
	rm -rf build out
