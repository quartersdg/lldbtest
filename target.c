#include <stdio.h>

void more() {
    static int x;
    printf("%x\n",x++);
}

void next() {
    static int x;
    printf("%x\n",x++);
    more(); more();
}

int main(int argc, char *argv[]) {
    int x = 0;
    volatile int done = 0;
    printf("Hello, World!\n");
    while (!done) {
        x++;
        next(); next();
        sleep(1);
    }
    return 0;
}
