
LOCAL = $(PWD)

include ../Makefile-arch.inc
ifeq ($(ARCH),mac)
	export CFLAGS = \
		-mmacosx-version-min=10.7
	export CXXFLAGS = $(CFLAGS)
	export LDFLAGS = $(CFLAGS)
endif

sdl2 = SDL2-2.0.7
libsndfile = libsndfile-1.0.28
libsamplerate = libsamplerate-0.1.9
fonts = Cantarell


# This instance of make should be serialized, but -j flags are passed down to each recursive Makefile
.NOTPARALLEL:

all: $(sdl2) $(jansson) $(libsndfile) $(libsamplerate) $(libcurl) $(fonts)

$(sdl2):
	wget -nc https://www.libsdl.org/release/$@.tar.gz
	tar xf $@.tar.gz
	cd $@ && ./configure --prefix="$(LOCAL)" --enable-input-tslib=no
	$(MAKE) -C $@
	$(MAKE) -C $@ install

$(libsndfile):
	wget -nc http://www.mega-nerd.com/libsndfile/files/$@.tar.gz
	tar xf $@.tar.gz
	cd $@ && ./configure --prefix="$(LOCAL)"
	$(MAKE) -C $@
	$(MAKE) -C $@ install

$(libsamplerate):
	wget -nc http://www.mega-nerd.com/SRC/$@.tar.gz
	tar xf $@.tar.gz
	cd $@ && ./configure --prefix="$(LOCAL)"
	$(MAKE) -C $@
	$(MAKE) -C $@ install

$(fonts):
	mkdir -p fonts
	cd fonts && wget -O $@.zip "https://fonts.google.com/download?family=$@" && unzip -o $@.zip && rm $@.zip
	cp fonts/$@-Regular.ttf ../font.ttf

clean:
	git clean -fdx
