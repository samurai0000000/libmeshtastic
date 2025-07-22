# Makefile
#
# Copyright (C) 2025, Charles Chiou

MAKEFLAGS =	--no-print-dir

TARGETS +=	build/libmeshtastic.a

.PHONY: default clean distclean $(TARGETS)

default: $(TARGETS)

clean:
	@test -f build/Makefile && $(MAKE) -C build clean

distclean:
	rm -rf build

.PHONY: libmeshtastic

libmeshtastic: build/libmeshtastic.a

build/libmeshtastic.a: build/Makefile
	@$(MAKE) -C build

build/Makefile: CMakeLists.txt
	@mkdir -p build
	@cd build && cmake ..
