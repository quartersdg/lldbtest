test: main.cc Makefile
	g++ -std=c++11 `pkg-config --cflags gtk+-3.0` -o test main.cc `pkg-config --libs gtk+-3.0` -g -O0 -I/usr/lib/llvm-3.6/include/ -llldb-3.6 -Wno-deprecated-declarations

