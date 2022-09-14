ALL: 03_example_of_readme.ino.with_bootloader.hex

color: 03_example_of_readme.ino
	./am2 03_example_of_readme.ino

03_example_of_readme.ino.with_bootloader.hex: 03_example_of_readme.ino
	GCC_COLORS="" ./am2 --no-color 03_example_of_readme.ino

clean:
	rm -rf build

mrproper:
	rm -rf build out
