#include <stdio.h>
#include <malloc.h>

#include "storage.h"

int main(int argc, char* argv[]) {
    if (argc != 3 ) {
        fprintf(stderr, "Usage: fsfs-read directory name  > output\n");
        return 1;
    }
    
    const char* dirname = argv[1];
    const char* basename = argv[2];
        
    struct storage__file* f = storage__open(dirname, basename);
    int len = storage__get_block_size(f);
    unsigned char *buf = (unsigned char*)malloc(len);
    int ret;
    long long int i;
    long long int mx = storage__get_number_of_blocks(f);
    for(i=0;i<mx;++i) {
        storage__read_block(f, buf, i);
        ret = fwrite(buf, len, 1, stdout);
        if(ret!=1) break;
    }
    storage__close(f);
    free(buf);
    return 0;
}
