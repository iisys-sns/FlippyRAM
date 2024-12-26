CC=g++
CFLAGS=-Wall -O3 -g
LDFLAGS= -pthread

.PHONY: default
default:bin/amdre

.PHONY: run
run: bin/amdre
	./bin/amdre

bin/amdre: build/amdre.o build/helper.o build/addressGroup.o build/bankGroup.o build/addressFunction.o build/maskThread.o build/config.o build/logger.o
	$(CC) $(LDFLAGS) -o $@ $^

build/%.o: %.cpp %.h
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	-rm build/*

.PHONY: cleanall
cleanall: clean
	-rm bin/*
