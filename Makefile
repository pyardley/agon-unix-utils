.PHONY: all clean install

export PATH := $(HOME)/development/tools/agdev/bin:$(PATH)

all:
	$(MAKE) -C cut
	$(MAKE) -C uniq
	mkdir -p bin mos
	cp -f cut/bin/cut.bin bin/cut.bin
	cp -f uniq/bin/uniq.bin bin/uniq.bin
	cp -f cut/bin/cut.bin mos/cut.bin
	cp -f uniq/bin/uniq.bin mos/uniq.bin

clean:
	$(MAKE) -C cut clean || true
	$(MAKE) -C uniq clean || true
	rm -f bin/cut.bin bin/uniq.bin mos/cut.bin mos/uniq.bin

install: all
	cp -f mos/cut.bin mos/uniq.bin $(HOME)/.agon-sdcard/mos/
