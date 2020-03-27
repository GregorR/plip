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
	cd gui ; $(MAKE) install

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
	mv plip/lib/plip-gui plip/gui
	rmdir plip/lib
	mv plip/bin/plip-gui plip/
	cp monitor/obs/obs-plip-monitor.so plip/
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
	
	cp processing/*.exe plip/bin/
	$(CROSS_PREFIX)strip plip/bin/*.exe
	mv plip/bin/plip-gui.exe plip/
	cp -a gui/out/plip-gui* plip/gui
	cat gui/LICENSE gui/out/plip-gui*/LICENSE > plip/gui/LICENSE
	
	unzip -d plip/ ffmpeg-$(FFMPEG_VERSION)-win64-shared.zip
	mv plip/ffmpeg-$(FFMPEG_VERSION)-win64-shared/ plip/ffmpeg
	mv plip/ffmpeg/bin/* plip/bin/
	mv plip/ffmpeg/presets plip/
	
	unzip -d plip/bin/ audiowaveform-$(AUDIOWAVEFORM_VERSION)-win64.zip
	
	rm -f plip-$(PLIP_VERSION)-win.zip
	zip -r plip-$(PLIP_VERSION)-win.zip plip/
	rm -rf plip/
endif
