# WaveEditMiMo, based on Synthesis Technology WaveEdit

WaveEditMiMo is a fork of the wavetable and bank editor for the Synthesis Technology [E370](http://synthtech.com/eurorack/E370/) and [E352](http://synthtech.com/eurorack/E352/) Eurorack synthesizer modules. It is modified to produce wavetable banks which are compatible with the Audiothingies [MicroMonsta](https://www.audiothingies.com/product/micromonsta/).

I have not build it on Windows or OSX, because i use Linux. If you like to give it a try on Windows or OSX, please do so and leave me a message if it build successfully. Thank you.

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

	./WaveEditMiMo

You can even try your luck with building the polished distributable. Although this method is unsupported, it may work with some tweaks to the Makefile.

	make dist
