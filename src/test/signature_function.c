#include <stdio.h>

typedef void (*foo_t)(void);
void (*bar)(void); // directly

void foo_(void) {
    printf("Foo\n");
}

void bar_(void) {
    printf("Bar\n");
}

int main(void) {
    foo_t foo = foo_; // via a type definition
    foo();
    bar = bar_;
    bar();
    return 0;
}
