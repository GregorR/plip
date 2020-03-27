include Makefile.share

ifeq ($(CROSS_COMPILE),)
UNAME=$(shell uname -s)
ifeq ($(UNAME),Linux)
DIST_OS=linux
else
DIST_OS=$(OS)
endif
else
DIST_OS=$(OS)
endif

all: deps/deps processing/processing gui/gui

deps/deps:
	cd deps ; $(MAKE)

processing/processing: deps/deps
	cd processing ; $(MAKE)

gui/gui:
	cd gui ; $(MAKE)

install:
	cd processing ; $(MAKE) install

clean:
	cd deps ; $(MAKE) clean
	cd processing ; $(MAKE) clean
	cd gui ; $(MAKE) clean

ifneq ($(OS),win)
dist: all
	mkdir plip
	mkdir plip/bin
	git archive -o plip/plip-src.tar HEAD
	xz plip/plip-src.tar
	$(MAKE) install PREFIX="$$PWD/plip"
	cp monitor/obs/obs-plip-monitor.so plip/
	cp processing/plip-gui plip/
	cp -a gui/out/plip-gui* plip/gui
	cat gui/LICENSE gui/out/plip-gui*/LICENSE > plip/gui/LICENSE
	tar -Jcf plip-$(PLIP_VERSION)-$(DIST_OS).tar.xz plip/
	rm -rf plip/

else
dist: all
	mkdir plip
	mkdir plip/bin
	git archive -o plip/plip-src.zip HEAD
	cp monitor/obs/obs-plip-monitor.dll plip/
	cp /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll deps/pcre/.libs/libpcre-1.dll plip/bin/
	$(CROSS_PREFIX)strip processing/*.exe
	cp processing/*.exe plip/bin/
	cp -a gui/out/plip-gui* plip/gui
	cat gui/LICENSE gui/out/plip-gui*/LICENSE > plip/gui/LICENSE
	unzip ffmpeg-$(FFMPEG_VERSION)-win64-shared.zip
	mv ffmpeg-$(FFMPEG_VERSION)-win64-shared/ plip/ffmpeg
	mv plip/ffmpeg/bin/* plip/bin/
	mv plip/ffmpeg/presets plip/
	rm -f plip-$(PLIP_VERSION)-win.zip
	zip -r plip-$(PLIP_VERSION)-win.zip plip/
	rm -rf plip/
endif
