ALL: 01_generic.ino.with_bootloader.hex

color: 01_generic.ino
	./am2 01_generic.ino

01_generic.ino.with_bootloader.hex: 01_generic.ino
	GCC_COLORS="" ./am2 --no-color 01_generic.ino

clean:
	rm -rf build

mrproper:
	rm -rf build out
