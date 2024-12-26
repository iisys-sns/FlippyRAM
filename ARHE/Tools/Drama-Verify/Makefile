CC=g++
CFLAGS=-Wall -O3 -g
LDFLAGS= -pthread

.PHONY: default
default:bin/drama-verify

bin/drama-verify: build/helper.o build/config.o build/logger.o \
	build/addressGroup.o build/addressingFunctions.o \
	build/linearAddressingFunctions.o build/drama.o build/drama-verify.o
	$(CC) $(LDFLAGS) -o $@ $^

build/%.o: %.cpp %.h asm.h
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	-rm build/*

.PHONY: cleanall
cleanall: clean
	-rm bin/*
