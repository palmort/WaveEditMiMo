# WaveEditMiMo, based on Synthesis Technology WaveEdit

WaveEditMiMo is a fork of the wavetable and bank editor for the Synthesis Technology [E370](http://synthtech.com/eurorack/E370/) and [E352](http://synthtech.com/eurorack/E352/) Eurorack synthesizer modules. It is modified to produce wavetable banks which are compatible with the Audiothingies [MicroMonsta](https://www.audiothingies.com/product/micromonsta/).

I do not have the possibility to build WaveEditMiMo on OSX. If you like to give it a try on OSX, please do so and leave me a message if it build successfully. Thank you.

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

A sincere "Thank you!" to [Andrew Belt](https://github.com/AndrewBelt) for his tips and the great work with the original [WaveEdit](https://github.com/AndrewBelt/WaveEdit).
