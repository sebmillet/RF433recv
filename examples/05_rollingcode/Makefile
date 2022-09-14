ALL: 05_rollingcode.ino.with_bootloader.hex

color: 05_rollingcode.ino
	./am2 05_rollingcode.ino

05_rollingcode.ino.with_bootloader.hex: 05_rollingcode.ino
	GCC_COLORS="" ./am2 --no-color 05_rollingcode.ino

clean:
	rm -rf build

mrproper:
	rm -rf build out
