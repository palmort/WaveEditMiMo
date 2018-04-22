# WaveEditMiMo, based on Synthesis Technology WaveEdit

**Work in progress! Exported wavetable banks have a wrong bit rate.**

WaveEditMiMo is a fork of the wavetable and bank editor for the Synthesis Technology [E370](http://synthtech.com/eurorack/E370/) and [E352](http://synthtech.com/eurorack/E352/) Eurorack synthesizer modules. I'm trying to modify it, so it is able to produce wavetable banks which are compatible with the Audiothingies [MicroMonsta](https://www.audiothingies.com/product/micromonsta/).

### Building

Make dependencies with

	cd dep
	make

Clone the in-source dependencies.

	cd ..
	git submodule update --init --recursive

Compile the program. The Makefile will automatically detect your operating system.

	make

Launch the program.

	./WaveEdit

You can even try your luck with building the polished distributable. Although this method is unsupported, it may work with some tweaks to the Makefile.

	make dist
