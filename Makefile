VERSION = 0.1

FLAGS = -Wall -Wextra -Wno-unused-parameter -g -Wno-unused -O3 -march=nocona -ffast-math \
	-DVERSION=$(VERSION) -DPFFFT_SIMD_DISABLE \
	-I. -Iext -Iext/imgui -Idep/include -Idep/include/SDL2
CFLAGS =
CXXFLAGS = -std=c++11
LDFLAGS =


SOURCES = \
	ext/pffft/pffft.c \
	ext/lodepng/lodepng.cpp \
	ext/imgui/imgui.cpp \
	ext/imgui/imgui_draw.cpp \
	ext/imgui/imgui_demo.cpp \
	ext/imgui/examples/sdl_opengl2_example/imgui_impl_sdl.cpp \
	$(wildcard src/*.cpp)


# OS-specific
include Makefile-arch.inc
ifeq ($(ARCH),lin)
	# Linux
	FLAGS += -DARCH_LIN $(shell pkg-config --cflags gtk+-2.0)
	LDFLAGS += -static-libstdc++ -static-libgcc \
		-lGL -lpthread \
		-Ldep/lib -lSDL2 -lsamplerate -lsndfile \
		-lgtk-x11-2.0 -lgobject-2.0
	SOURCES += ext/osdialog/osdialog_gtk2.c
else ifeq ($(ARCH),mac)
	# Mac
	FLAGS += -DARCH_MAC \
		-mmacosx-version-min=10.7
	CXXFLAGS += -stdlib=libc++
	LDFLAGS += -mmacosx-version-min=10.7 \
		-stdlib=libc++ -lpthread \
		-framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo \
		-Ldep/lib -lSDL2 -lsamplerate -lsndfile
	SOURCES += ext/osdialog/osdialog_mac.m
else ifeq ($(ARCH),win)
	# Windows
	FLAGS += -DARCH_WIN
	LDFLAGS += \
		-Ldep/lib -lmingw32 -lSDL2main -lSDL2 -lsamplerate -lsndfile \
		-lopengl32 -mwindows
	SOURCES += ext/osdialog/osdialog_win.c
	OBJECTS += info.o
info.o: info.rc
	windres $^ $@
endif


.DEFAULT_GOAL := build
build: WaveEditMiMo

run: WaveEditMiMo
	LD_LIBRARY_PATH=dep/lib ./WaveEditMiMo

debug: WaveEditMiMo
ifeq ($(ARCH),mac)
	lldb ./WaveEditMiMo
else
	gdb -ex 'run' ./WaveEditMiMo
endif


OBJECTS += $(SOURCES:%=build/%.o)


WaveEditMiMo: $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -frv $(OBJECTS) WaveEditMiMo dist


.PHONY: dist
dist: WaveEditMiMo
	mkdir -p dist/WaveEditMiMo
	cp LICENSE* dist/WaveEditMiMo
	cp doc/manual.pdf dist/WaveEditMiMo
ifeq ($(ARCH),lin)
	cp -R logo.png logo.svg OFL.txt font.ttf dist/WaveEditMiMo
	cp WaveEditMiMo WaveEditMiMo.sh dist/WaveEditMiMo
	cp dep/lib/libSDL2-2.0.so.0 dist/WaveEditMiMo
	cp dep/lib/libsamplerate.so.0 dist/WaveEditMiMo
	cp dep/lib/libsndfile.so.1 dist/WaveEditMiMo
else ifeq ($(ARCH),mac)
	mkdir -p dist/WaveEditMiMo/WaveEditMiMo.app/Contents/MacOS
	mkdir -p dist/WaveEditMiMo/WaveEditMiMo.app/Contents/Resources
	cp Info.plist dist/WaveEditMiMo/WaveEditMiMo.app/Contents
	cp WaveEditMiMo dist/WaveEditMiMo/WaveEditMiMo.app/Contents/MacOS
	cp -R logo.icns OFL.txt font.ttf dist/WaveEditMiMo/WaveEditMiMo.app/Contents/Resources
	# Remap dylibs in executable
	otool -L dist/WaveEditMiMo/WaveEditMiMo.app/Contents/MacOS/WaveEditMiMo
	cp dep/lib/libSDL2-2.0.0.dylib dist/WaveEditMiMo/WaveEditMiMo.app/Contents/MacOS
	install_name_tool -change $(PWD)/dep/lib/libSDL2-2.0.0.dylib @executable_path/libSDL2-2.0.0.dylib dist/WaveEditMiMo/WaveEditMiMo.app/Contents/MacOS/WaveEditMiMo
	cp dep/lib/libsamplerate.0.dylib dist/WaveEditMiMo/WaveEditMiMo.app/Contents/MacOS
	install_name_tool -change $(PWD)/dep/lib/libsamplerate.0.dylib @executable_path/libsamplerate.0.dylib dist/WaveEditMiMo/WaveEditMiMo.app/Contents/MacOS/WaveEditMiMo
	cp dep/lib/libsndfile.1.dylib dist/WaveEditMiMo/WaveEditMiMo.app/Contents/MacOS
	install_name_tool -change $(PWD)/dep/lib/libsndfile.1.dylib @executable_path/libsndfile.1.dylib dist/WaveEditMiMo/WaveEditMiMo.app/Contents/MacOS/WaveEditMiMo
	otool -L dist/WaveEditMiMo/WaveEditMiMo.app/Contents/MacOS/WaveEditMiMo
else ifeq ($(ARCH),win)
	cp font.ttf OFL.txt dist/WaveEditMiMo
	cp WaveEditMiMo.exe dist/WaveEditMiMo
	cp /mingw32/bin/libgcc_s_dw2-1.dll dist/WaveEditMiMo
	cp /mingw32/bin/libwinpthread-1.dll dist/WaveEditMiMo
	cp /mingw32/bin/libstdc++-6.dll dist/WaveEditMiMo
	cp dep/bin/SDL2.dll dist/WaveEditMiMo
	cp dep/bin/libsamplerate-0.dll dist/WaveEditMiMo
	cp dep/bin/libsndfile-1.dll dist/WaveEditMiMo
endif
	cd dist && zip -9 -r WaveEditMiMo-$(VERSION)-$(ARCH).zip WaveEditMiMo


# SUFFIXES:

build/%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(FLAGS) $(CFLAGS) -c -o $@ $<

build/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(FLAGS) $(CXXFLAGS) -c -o $@ $<

build/%.m.o: %.m
	@mkdir -p $(@D)
	$(CC) $(FLAGS) $(CFLAGS) -c -o $@ $<
