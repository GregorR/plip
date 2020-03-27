include ../Makefile.share

ifeq ($(OS),win)
PCRE_FLAGS=
else
PCRE_FLAGS=--enable-static --disable-shared
endif

all:
	cd gc ; $(MAKE) -f Makefile.direct cords CROSS_PREFIX="$(CROSS_PREFIX)"
	cd pcre ; \
		( test -e Makefile || ./configure --host="$(CROSS_COMPILE)" $(PCRE_FLAGS) ) ; \
		$(MAKE)
	cd speexdsp ; \
		( test -e configure || ./autogen.sh ) ; \
		( test -e Makefile || ./configure --host="$(CROSS_COMPILE)" --enable-static --disable-shared ) ; \
		$(MAKE)

clean:
	cd gc ; $(MAKE) -f Makefile.direct clean
	cd pcre ; \
		( test ! -e Makefile || $(MAKE) distclean )
	cd speexdsp ; \
		( test ! -e Makefile || $(MAKE) distclean )
