#include <stdio.h>
#include <endian.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <lzo/lzo1x.h>
#include "hash.h"
#include "storage.h"

int main(int argc, char* argv[]) {
    if(argc<2 || !strcmp(argv[1], "--help")) {
        fprintf(stderr, "Usage:\n"
            "    fsfs-debug print-index block_size blockgroup_size file.idx [bgstart [bgcount]]\n"
            "    fsfs-debug comp-stats blockgroup_size file.idx\n"
            "    fsfs-debug decompress-block file.dat offset compressed_size > output\n"
            "    fsfs-debug decompress-block2 < input > output\n"
            "    fsfs-debug compress-block < input > output\n"
            "    fsfs-debug compress-block2 < input > output\n"
            "    fsfs-debug calculate-hash < input\n"
            "    fsfs-debug get-length dir name\n"
            "    fsfs-debug read-one-block dir name blocknum\n"
        );
        return 1;   
    }
    if(!strcmp(argv[1], "print-index")) {
        int bgsize=1020;
        int block_size=4096;
        long long int bgstart = 0;
        long long int bgcount = -1;
        assert(argc>=5 && argc<=7);
        sscanf(argv[2], "%d", &block_size);
        sscanf(argv[3], "%d", &bgsize);
        const char* idxfile = argv[4];
        if(argc>=6) sscanf(argv[5], "%lld", &bgstart);
        if(argc>=7) sscanf(argv[6], "%lld", &bgcount);
        
        int bglen = bgsize * 2 + 8;
        
        FILE* idx = stdin;
        if(strcmp(argv[2], "-")) idx = fopen(idxfile, "rb");
        
        assert(idx!=NULL);
        
        signed short int q;
        long long int baseoffset;
        
        if(bgstart!=0) { 
            int ret = fseek(idx, bgstart*bglen, SEEK_SET); 
            if(ret) { perror("fseek"); return 2;}
        }
        
        while(bgcount!=0) {
            int ret = fread(&baseoffset, 1, 8, idx);
            if(ret==0)break;
            if(ret!=8) {
                printf("Trimmed index file\n");
                return 2;
            }
            baseoffset = be64toh(baseoffset);
            printf("Block group %lld, base offset: 0x%016llX\n", bgstart, baseoffset);
            
            int i;
            int accum = 0;
            for(i=0; i<bgsize; ++i) {
                int ret = fread(&q, 1, 2, idx);
                if(ret!=2) {
                    printf("Trimmed index file\n");
                    return 2;
                }
                q = be16toh(q);
                
                printf("block %lld: ", i + bgstart*bgsize);
                if(q==-0x8000) {
                    printf("zero\n");
                } else
                if(q==-0x7FFF) {
                    printf("uncompressed (%d bytes) at 0x%016llX\n", block_size, baseoffset+accum);
                    accum+=block_size;
                } else
                if(q==0) {
                    printf("unallocated\n");
                } else
                if(q>0 && q<0x4444) {
                    printf("compressed (%d bytes) at 0x%016llX\n", q, baseoffset+accum);
                    accum+=q;
                } else
                if (q<0 && q >= -64) {
                    printf("reference to %d's dependency\n", (-q)-1);
                } else {
                    printf("probably invalid (%04X)\n", q);
                }
            }
                
            fflush(stdout);
            --bgcount;   
            ++bgstart;
        }
        
        return 0;  
    } else
    if(!strcmp(argv[1], "comp-stats")) {
        int bgsize=1020;
        assert(argc==4);
        sscanf(argv[2], "%d", &bgsize);
        const char* idxfile = argv[3];
        
        FILE* idx = stdin;
        if(strcmp(argv[2], "-")) idx = fopen(idxfile, "rb");
        
        assert(idx!=NULL);
        
        long long int baseoffset;
        signed short int q;
        
        unsigned long long int *stats = (unsigned long long int*) malloc(8*32768);
        unsigned long long int zeroes = 0;
        unsigned long long int invals = 0;
        unsigned long long int refs[64];
        unsigned long long int total = 0;
        unsigned long long int uncompressibles = 0;
        memset(&refs, 0, sizeof(refs));
        memset(stats, 0, 8*32768);
        
        int i;
        int trailing_zero_counter=0;
        for(;;) {
            int ret = fread(&baseoffset, 1, 8, idx);
            if(ret!=8)break;
            baseoffset = be64toh(baseoffset);
            
            for(i=0; i<bgsize; ++i) {
                int ret = fread(&q, 1, 2, idx);
                if(ret!=2) return 2;
                q = be16toh(q);
                
                if(q>0) {
                    ++stats[q];
                }else
                if(q==-0x7FFF) {
                    ++uncompressibles;
                }else
                if(q==-0x8000) {
                    ++zeroes;
                }else
                if(q==0) {
                    ++trailing_zero_counter;
                }else
                if(q<0 && q>=-64) {
                    ++refs[(-q)-1];
                }else{
                    ++invals;
                }
                ++total;
            }
        }
        
        total-=trailing_zero_counter;
        
        long long int running = 0;
        
        printf("total: %lld (100%%) ; 0%% \n", total);
        
        running+=zeroes;
        if(zeroes>0)printf("zero: %lld (%g%%) ; %g%%\n",   zeroes, 
            100.0*zeroes/total, 100.0*running/total);
        
        for(i=0; i<64; ++i) {
            running+=refs[i];
            if(refs[i]>0)printf("refs[%d]: %lld (%g%%) ; %g%%\n",  i, refs[i], 
                    100.0*refs[i]/total, 100.0*running/total);
        }
        for(i=0; i<32768; ++i) {
            running+=stats[i];
            if(stats[i]>0)printf("compressed[%d]: %lld (%g%%) ; %g%%\n",  i, stats[i],
                    100.0*stats[i]/total, 100.0*running/total);
        }
        running+=uncompressibles;
        if(uncompressibles>0)printf("uncompressible: %lld (%g%%) ; %g%%\n",   uncompressibles, 
            100.0*uncompressibles/total, 100.0*running/total);
        
        running+=invals;
        if(invals>0)printf("invalid: %lld (%g%%) ; %g%%\n",   invals, 
                100.0*invals/total, 100.0*running/total);
        
        free(stats);
        return 0;
    } else
    if(!strcmp(argv[1], "decompress-block")) {
        assert(argc==5);
        
        const char* datfile = argv[2];
        long long int offset = 0;
        int size;
        
        sscanf(argv[3], "%lld", &offset);
        sscanf(argv[4], "%d", &size);
        
        FILE* dat = fopen(datfile, "rb");
        
        int ret = fseek(dat, offset, SEEK_SET);
        assert(ret==0);
        
        assert(size<65536);
        
        unsigned char chunk[65536];
        unsigned char chunk2[65536+2048];
        
        ret = fread(&chunk, 1, size, dat);
        assert(ret==size);
        fclose(dat);
        
        lzo_uint len = 65536+2048;
        lzo1x_decompress_safe(chunk, ret, chunk2, &len, NULL);
        
        fwrite(chunk2, 1, len, stdout);
        
        
        return 0;
    } else
    if(!strcmp(argv[1], "decompress-block2")) {
        unsigned char chunk[65536];
        unsigned char chunk2[65536+2048];
        
        int ret = fread(&chunk, 1, 65536, stdin);
        
        lzo_uint len = 65536+2048;
        lzo1x_decompress_safe(chunk, ret, chunk2, &len, NULL);
        
        fwrite(chunk2, 1, len, stdout);
        return 0;
    } else
    if(!strcmp(argv[1], "compress-block")) {
        unsigned char chunk[65536];
        unsigned char chunk2[65536+2048];
        int ret = fread(&chunk, 1, 65536, stdin);
        
        char tmp[LZO1X_1_MEM_COMPRESS];
        lzo_uint len = 65536+2048;
        lzo1x_1_compress(chunk, ret, chunk2, &len, &tmp);
        
        fwrite(chunk2, 1, len, stdout);
        
        return 0;
    } else
    if(!strcmp(argv[1], "compress-block2")) {
        unsigned char chunk[65536];
        unsigned char chunk2[65536+2048];
        int ret = fread(&chunk, 1, 65536, stdin);
        
        char tmp[LZO1X_999_MEM_COMPRESS];
        lzo_uint len = 65536+2048;
        lzo1x_999_compress(chunk, ret, chunk2, &len, &tmp);
        
        fwrite(chunk2, 1, len, stdout);
        
        return 0;
    } else
    if(!strcmp(argv[1], "calculate-hash")) {
        unsigned char chunk[65536];
        int ret = fread(&chunk, 1, 65536, stdin);
        unsigned char c = phash(chunk, ret);
        printf("%02x\n", c);
        return 0;
    } else
    if(!strcmp(argv[1], "get-length")) {
        assert(argc==4);
        const char* dirname = argv[2];
        const char* basename = argv[3];
        int block_len = storage__get_block_size2(dirname, basename);
        long long int number_of_blocks = storage__get_number_of_blocks2(dirname, basename);
        printf("%lld\n", number_of_blocks*block_len);
        return 0;
    } else
    if(!strcmp(argv[1], "read-one-block")) {
        assert(argc==5);
        const char* dirname = argv[2];
        const char* basename = argv[3];
        long long int blocknum;
        sscanf(argv[4], "%lld", &blocknum);
        struct storage__file* f = storage__open(dirname, basename);
        int block_len = storage__get_block_size(f);
        
        unsigned char buf[65536];
        storage__read_block(f, buf, blocknum);
        fwrite(buf, 1, block_len, stdout);
        return 0;
    } else {
        fprintf(stderr, "Unknown command %s\n", argv[1]);
        return 1;
    } 
}
