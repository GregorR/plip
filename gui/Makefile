include ../Makefile.share

all: out/make

ifneq ($(OS),win)
out/make: node_modules/.bin/electron-forge
	node_modules/.bin/electron-forge make

else
out/make: node_modules/.bin/electron-forge
	node_modules/.bin/electron-forge make -p win32
endif

node_modules/.bin/electron-forge:
	npm install

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/lib/plip-gui
	cp -a out/plip-gui*/* $(DESTDIR)$(PREFIX)/lib/plip-gui/
	cat LICENSE out/plip-gui*/LICENSE > $(DESTDIR)$(PREFIX)/lib/plip-gui/LICENSE

clean:
	rm -rf out

distclean: clean
	rm -rf node_modules

.PRECIOUS: node_modules/.bin/electron-forge
