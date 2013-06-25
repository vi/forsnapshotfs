#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include "storage.h"


int main(int argc, char* argv[]) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: forsnapshotfs/write directory name [previous_name] < input\n");
        return 1;
    }
    
    const char* dirname = argv[1];
    const char* basename = argv[2];
    const char* depname = argv[3];
    
    { // check for already existing file
        char namebuf[4096];
        snprintf(namebuf, 4096, "%s/%s.idx", dirname, basename);
        FILE* existing_file = fopen(namebuf, "r");
        if(existing_file) {
            fprintf(stderr, "%s already exists, refusing to overwrite\n", namebuf);
            return 3;
        }
    }
    
    struct storage__file* f = storage__creat(dirname, basename, depname);
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
    long long int snew,sreused,shashcoll,szero,sdblref;
    storage__get_writestat(f, &snew, &sreused, &shashcoll, &szero, &sdblref);
    storage__close(f);
    free(buf);
    fprintf(stdout, 
        "Completed. new: %lld   reused: %lld   hashcoll: %lld  zero: %lld  dblref: %lld\n",
         snew, sreused, shashcoll, szero, sdblref);
    return 0;
}
