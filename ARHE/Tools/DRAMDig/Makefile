EXTFLAG=-DDRAMDIG_DEBUG

all: dramdig
dramdig: *.cc
	g++ -std=c++11 -g -O0 dramdig.cc utility.cc $(EXTFLAG) -o dramdig
clean:
	rm -rf dramdig
