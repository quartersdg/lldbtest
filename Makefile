test: main.cc Makefile stb_c_lexer.h
	g++ -std=c++11  -o test main.cc -g -O0 -I/usr/lib/llvm-3.6/include/ -llldb-3.6 -Wno-deprecated-declarations `pkg-config --cflags --libs gtk+-3.0`

