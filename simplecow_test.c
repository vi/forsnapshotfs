#include <stdio.h>
#include <string.h>

#include "simplecow.h"

char buf[36];

static int backing_read(void* usr, long long int off, int size, char* b) {
    memcpy(b, buf+off, size);
    return size;
}

void dump(struct simplecow *cow) {
    char bu[37];
    memcpy(bu, "------------------------------------", 37);
    simplecow_read(cow, 0, 36, bu);
    printf("%s\n", bu);
    
    memcpy(bu, "------------------------------------", 37);
    int i;
    for(i=0; i<36; ++i) {
        simplecow_read(cow, i, 1, bu+i);
    }
    printf("%s\n\n", bu);
    fflush(stdout);
}

int main() {
    memcpy(buf,"abcdefghijklmnopqrstuvwxyzabcdefghij", 36);
    struct simplecow *cow = simplecow_create(&backing_read, NULL);
    
    dump(cow);
    simplecow_write(cow, 4, 8, "EFGHIJKL");
    dump(cow);
    simplecow_write(cow, 6, 2, "55");
    dump(cow);
    simplecow_write(cow, 1, 6, "000000");
    dump(cow);
    simplecow_write(cow, 6, 6, "111111");
    dump(cow);
    simplecow_write(cow, 6, 1, "2");
    dump(cow);
    simplecow_write(cow, 0, 36, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    dump(cow);
    {int i; for(i=0; i<18; ++i) {  simplecow_write(cow, i*2, 1, "B"); } }
    dump(cow);
    {int i; for(i=0; i<12; ++i) {  simplecow_write(cow, i*3, 2, "CC"); } }
    dump(cow);
    
    simplecow_destroy(cow);
    return 0;
}
