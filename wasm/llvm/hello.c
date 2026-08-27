volatile unsigned short counter;

unsigned short add16(unsigned short a, unsigned short b) {
    return (unsigned short)(a + b);
}

void tick(void) {
    counter = add16(counter, 1u);
}
