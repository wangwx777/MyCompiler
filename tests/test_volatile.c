int read_volatile(volatile int *reg) { return *reg; }
void write_volatile(volatile int *reg, int val) { *reg = val; }
