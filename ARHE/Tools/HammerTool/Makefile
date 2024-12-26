CC=gcc
CFLAGS=-Wall

.PHONY: default
default: bin/hammertool bin/flipper

bin/hammertool: build/memoryInspect.o build/pfnInspect.o build/util.o build/afunc.o build/hammer.o build/hammerutil.o build/hammertool.o
	$(CC) -pthread -o $@ $^

bin/flipper: build/flipper.o
	$(CC) -pthread $(LDFLAGS) -o $@ $^

build/hammertool.o: hammertool.c hammertool.h asm.h
	$(CC) -DVERSION=\"$(shell date +%Y-%m-%d)\" $(CFLAGS) -c -o $@ $<

build/%.o: %.c %.h asm.h
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY : clean
clean:
	rm build/*

.PHONY : cleanall
cleanall:clean
	rm bin/*
