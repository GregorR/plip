include ../Makefile.share

EXE_EXT=

ifeq ($(OS),win)
EXE_EXT=.exe
PCRE=../deps/pcre/.libs/libpcre.dll.a
else
PCRE=../deps/pcre/.libs/libpcre.a
endif

ifeq ($(OS),win)
THREADS=-lpthread
THREADS_STATIC=-Wl,-Bstatic -lpthread -Wl,-Bdynamic
MWINDOWS=-mwindows
else
THREADS=-pthread
THREADS_STATIC=$(THREADS)
MWINDOWS=
endif

LIBS=../deps/gc/gc.a $(PCRE) $(THREADS)

EXES= \
	plip$(EXE_EXT) \
	plip-demux$(EXE_EXT) \
	plip-findnoise$(EXE_EXT) \
	plip-speexdenoise$(EXE_EXT) \
	plip-noiserepellentdenoise$(EXE_EXT) \
	plip-aproc$(EXE_EXT) \
	plip-marktofilter$(EXE_EXT) \
	plip-clip$(EXE_EXT) \
	plip-mix$(EXE_EXT) \
	plip-defconfig$(EXE_EXT) \
	plip-gui$(EXE_EXT)

all: $(EXES)

plip$(EXE_EXT): plip-launcher$(EXE_EXT)
	cp $< $@

plip-%$(EXE_EXT): %.c ../share/cscript.c hashtable.c configfile.c defconfig.h
	$(CC) -std=c99 $(CFLAGS) \
		$< ../share/cscript.c hashtable.c configfile.c \
		-I ../share -I ../deps/gc/include -I ../deps/pcre \
		$(LIBS) \
		-o $@

plip-speexdenoise$(EXE_EXT): speexdenoise.c
	$(CC) -std=c99 $(CFLAGS) \
		-I ../share -I ../deps/speexdsp/include \
		speexdenoise.c \
		../deps/speexdsp/libspeexdsp/.libs/libspeexdsp.a -lm \
		-o $@

plip-noiserepellentdenoise$(EXE_EXT): noiserepellentdenoise.c
	$(CC) -std=c99 $(CFLAGS) \
		-I ../share -I ../deps/noise-repellent/src \
		noiserepellentdenoise.c \
		../deps/noise-repellent/src/libnr.a \
		../deps/fftw/.libs/libfftw3f.a \
		-lm \
		-o $@

plip-gui$(EXE_EXT): gui.c
	$(CC) -std=c99 $(CFLAGS) -DNO_PCRE $(MWINDOWS) \
		$< ../share/cscript.c \
		-I ../share -I ../deps/gc/include \
		../deps/gc/gc.a $(THREADS_STATIC) \
		-o $@

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	for i in $(EXES); do install -s $$i $(DESTDIR)$(PREFIX)/bin/$$i; done

clean:
	rm -f $(EXES) plip-launcher$(EXE_EXT)
