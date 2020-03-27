include ../Makefile.share

ifeq ($(OS),win)
PCRE_FLAGS=
else
PCRE_FLAGS=--enable-static --disable-shared
endif

all: gc/gc.a pcre/.libs/libpcre.a speexdsp/libspeexdsp/.libs/libspeexdsp.a fftw/.libs/libfftw3f.a noise-repellent/src/libnr.a

gc/gc.a:
	cd gc ; $(MAKE) -f Makefile.direct cords CROSS_PREFIX="$(CROSS_PREFIX)"

pcre/.libs/libpcre.a:
	cd pcre ; \
		( test -e Makefile || ./configure --host="$(CROSS_COMPILE)" $(PCRE_FLAGS) ) ; \
		$(MAKE)

speexdsp/libspeexdsp/.libs/libspeexdsp.a:
	cd speexdsp ; \
		( test -e configure || ./autogen.sh ) ; \
		( test -e Makefile || ./configure --host="$(CROSS_COMPILE)" --enable-static --disable-shared ) ; \
		$(MAKE)

fftw/.libs/libfftw3f.a:
	( test -e fftw-$(FFTW_VERSION).tar.gz || wget http://www.fftw.org/fftw-$(FFTW_VERSION).tar.gz )
	( test -e fftw || ( \
		tar xf fftw-$(FFTW_VERSION).tar.gz ; \
		mv fftw-$(FFTW_VERSION) fftw \
	) \
	)
	cd fftw ; \
		( test -e Makefile || ./configure --host="$(CROSS_COMPILE)" --enable-float --enable-static --disable-shared ) ; \
		$(MAKE)

noise-repellent/src/libnr.a:
	cd noise-repellent/src ; $(MAKE) CROSS_PREFIX="$(CROSS_PREFIX)"

clean:
	cd gc ; $(MAKE) -f Makefile.direct clean
	cd pcre ; \
		( test ! -e Makefile || $(MAKE) distclean )
	cd speexdsp ; \
		( test ! -e Makefile || $(MAKE) distclean )
	test ! -e fftw || rm -rf fftw
	cd noise-repellent/src ; $(MAKE) clean
