all: asynctest target

asynctest: asynctest.cc Makefile filemap.cc lldb.cc
	g++ -std=c++11 -O0 -g -o $@ $< `pkg-config --cflags --libs sdl2 gl` -I/usr/lib/llvm-3.6/include/ -L/usr/lib/llvm-3.6/lib/ -llldb-3.6

target: target.c
	gcc -O0 -g -o $@ $^

clean:
	rm asynctest target
