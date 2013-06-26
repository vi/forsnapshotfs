#include <stdio.h>
#include <malloc.h>

#include "storage.h"
#include "hash.h"

int main(int argc, char* argv[]) {
    if (argc != 3 ) {
        fprintf(stderr, "Usage: fsfs-verify directory name\n");
        return 1;
    }
    
    const char* dirname = argv[1];
    const char* basename = argv[2];
        
    struct storage__file* f = storage__open(dirname, basename);
    int len = storage__get_block_size(f);
    unsigned char *buf = (unsigned char*)malloc(len);
    long long int i;
    long long int mx = storage__get_number_of_blocks(f);
    for(i=0;i<mx;++i) {
        storage__read_block(f, buf, i);
        unsigned char h1 = phash(buf, len);
        unsigned char h2 = storage__get_block_hash(f, i);
        if(h1!=h2) {
            fprintf(stderr, "Block %lld hash mismatch\n", i);
            return 2;
        }
    }
    storage__close(f);
    free(buf);
    return 0;
}
