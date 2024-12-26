CC=gcc
CFLAGS=-Wall -g
LDFLAGS=

.PHONY: default
default: bin/measureRefreshRate

bin/measureRefreshRate: build/measureRefreshRate.o | bin
	$(CC) $(LDFLAGS) -o $@ $^

build/%.o: %.c %.h | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build
bin:
	mkdir -p bin
.PHONY: clean
clean:
	-rm -f build/*

.PHONY: cleanall
cleanall: clean
	-rm -f bin/*
