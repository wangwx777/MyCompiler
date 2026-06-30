// Test: ADDB (i8), ADDH (i16), ADDW (i32)
// llc: build/bin/llc -march=mycpu -filetype=asm -o test_add.s test_add.ll

char add_char(char a, char b)       { return a + b; }   // add.b
short add_short(short a, short b)   { return a + b; }   // add.h
int add_int(int a, int b)           { return a + b; }   // add.w
