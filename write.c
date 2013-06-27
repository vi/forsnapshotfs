#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>

#include "storage.h"


int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: fsfs-write [-9] directory name [previous_name|NULL [block_size [block_group_size]]] < input\n");
        fprintf(stderr, "   \n");
        return 1;
    }
    
    int best_compression = 0;
    
    if(!strcmp(argv[1],"-9")) {
        best_compression = 1;
        --argc;
        ++argv;
    }
    
    const char* dirname = argv[1];
    const char* basename = argv[2];
    assert(basename!=NULL);
    const char* depname = argv[3];
    
    
    int block_size = 4096;
    int block_group_size = 1020;
    
    if(argc>=5)sscanf(argv[4],"%d",&block_size);
    if(argc>=6)sscanf(argv[5],"%d",&block_group_size);
    
    if(depname && !strcmp(depname,"NULL")) depname=NULL;
        
    { // check for already existing file
        char namebuf[4096];
        snprintf(namebuf, 4096, "%s/%s.idx", dirname, basename);
        FILE* existing_file = fopen(namebuf, "r");
        if(existing_file) {
            fprintf(stderr, "%s already exists, refusing to overwrite\n", namebuf);
            return 3;
        }
    }
    
    struct storage__file* f = storage__creat(dirname, basename, depname, 
                                             block_size, block_group_size, best_compression);
    int len = storage__get_block_size(f);
    unsigned char *buf = (unsigned char*)malloc(len);
    int ret;
    for(;;) {
        ret = fread(buf, 1, len, stdin);
        if(ret!=len) {
            if(ret>0) {
                memset(buf+ret, 0, len-ret);
                storage__append_block(f, buf);
            }
            break;
        }
        storage__append_block(f, buf);
    }
    long long int scompressed,suncompressible,sreused,shashcoll,szero,sdblref;
    storage__get_writestat(f, &scompressed, &suncompressible, &sreused, &shashcoll, &szero, &sdblref);
    storage__close(f);
    free(buf);
    fprintf(stdout, 
        "Completed. compressed: %lld   uncompressible: %lld  reused: %lld   hashcoll: %lld  zero: %lld  dblref: %lld\n",
         scompressed, suncompressible, sreused, shashcoll, szero, sdblref);
    return 0;
}
