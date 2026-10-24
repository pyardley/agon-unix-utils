.PHONY: all clean install test

export PATH := $(HOME)/development/tools/agdev/bin:$(PATH)

UTILS = cut uniq touch tee tr diff du df

all:
	@for u in $(UTILS); do $(MAKE) -C $$u; done
	mkdir -p bin mos
	@for u in $(UTILS); do \
		cp -f $$u/bin/$$u.bin bin/$$u.bin; \
		cp -f $$u/bin/$$u.bin mos/$$u.bin; \
	done

clean:
	@for u in $(UTILS); do $(MAKE) -C $$u clean || true; done
	rm -f bin/*.bin mos/*.bin
	rm -rf tests/sdcard
	rm -rf $(HOME)/.agon-sdcard/_test_agon_unix_utils

install: all
	cp -f mos/*.bin $(HOME)/.agon-sdcard/mos/
	@if [ -d third_party/vascocosta ]; then \
		cp -f third_party/vascocosta/head.bin third_party/vascocosta/tail.bin \
			$(HOME)/.agon-sdcard/mos/; \
	fi

test: all
	./tests/run-emulator-tests.sh
