#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE

#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <endian.h>

#include <lzo/lzo1x.h>
#include "hash.h"
#include "storage.h"

#define CHUNK 65536

struct storage__file {
    int block_size;
    int block_group_size;
    long long int current_block; // block to be written next == number of blocks
    
    FILE* data_file; // compressed blocks
    FILE* index_file; // compressed blocks offsets
    FILE* description_file;
    FILE* hash_file;
    
    struct index_entry {
        unsigned long long int base_offset;
        signed short int offsets[1]; // [0]
    }* current_index_entry;
    
    unsigned char *outbuf;
    
    struct storage__file* deps[64];
    int depscount;
    
    int opened_for_writing;
    int best_compression;
    
    long long int writestat_compressed;
    long long int writestat_uncompressible;
    long long int writestat_reused;
    long long int writestat_hashcoll;
    long long int writestat_zero;
    long long int writestat_dblrefs;
};

static struct storage__file* storage__open_nodeps(const char* dirname, const char* basename);
    
static int storage__get_dep_name(
        const char* dirname, const char* basename, char* depname_buf, size_t buflen) {

    char namebuf[4096];
    snprintf(namebuf, 4096, "%s/%s.dsc", dirname, basename);
    FILE* ascfile = fopen(namebuf, "rb");
    assert(ascfile != NULL);
    fscanf(ascfile, "%*d%*d\n");
    memset(depname_buf, 0, buflen);
    int ret = fread(depname_buf, 1, buflen, ascfile);
    fclose(ascfile);
    return ret;
}
    
static void open_deps(struct storage__file* c, const char* dirname, const char* depname) {
    c->depscount = 0;
    if (!depname) {
        return;
    }
    struct storage__file* dep = storage__open_nodeps(dirname, depname);
    c->deps[c->depscount++] = dep;
    
    char namebuf[4096];
    strncpy(namebuf, depname, 4096);
    
    int ret;
    for(;;) {
        ret = storage__get_dep_name(dirname, namebuf,   namebuf, 4096);
        if(ret==0)break;
            
        //fprintf(stderr, "Additional storage: %s\n", namebuf);
        struct storage__file* dep = storage__open_nodeps(dirname, namebuf);
        assert(dep->block_size == c->block_size);
        c->deps[c->depscount++] = dep;
        if(c->depscount==64) break;
    }
}

static size_t get_index_entry_size(const struct storage__file* c) {
    return 8+c->block_group_size*2;
    // base offset + lengths
}

size_t storage__get_block_size(const struct storage__file* c) {
    return c->block_size;
}

static long long int get_number_of_blocks(FILE* index_file, int block_group_size) {
    fseeko(index_file, 0, SEEK_END);
    off_t endpos = ftello(index_file);
    
    int len = 8+block_group_size*2;
    
    long long int number_of_block_groups = endpos / len;
    
    if (number_of_block_groups == 0) {
        return 0;        
    }
    
    fseek(index_file, (number_of_block_groups-1)*len+8, SEEK_SET);
    int j=0; // number of consecutive empty blocks
    short int ll;
    int i;
    /* exclude zero blocks at the end */
    for(i=0; i<block_group_size; ++i) {
        fread(&ll, 2, 1, index_file);
        if(ll) {
            j=0;
        } else {
            ++j;
        }
    }
    // not j is number of zero blocks at the end of index
    
    return (number_of_block_groups-1) * block_group_size + (block_group_size-j);
}

long long int storage__get_number_of_blocks(const struct storage__file* c) {
    off_t prevpos = ftello(c->index_file);
    
    long long int number_of_blocks = get_number_of_blocks(c->index_file, c->block_group_size);
    
    fseeko(c->index_file, prevpos, SEEK_SET);
    
    return number_of_blocks;
}

long long int storage__get_number_of_blocks2(const char* dirname, const char* basename) {
    int block_size, block_group_size;
    
    
    char namebuf[4096];
    snprintf(namebuf, 4096, "%s/%s.dsc", dirname, basename);
    FILE* dsc_file = fopen(namebuf, "rb");
    if(dsc_file==NULL)return -1;
    
    int ret = fscanf(dsc_file,"%d%d\n", &block_size, &block_group_size);
    assert(ret==2);
    
    fclose(dsc_file);
    snprintf(namebuf, 4096, "%s/%s.idx", dirname, basename);
    FILE* index_file = fopen(namebuf, "rb");
    if(index_file==NULL)return -1;
    

    long long int number_of_blocks = get_number_of_blocks(index_file, block_group_size);
    
    fclose(index_file);
    
    return number_of_blocks;
}


size_t storage__get_block_size2(const char* dirname, const char* basename) {
    int block_size, block_group_size;
    
    
    char namebuf[4096];
    snprintf(namebuf, 4096, "%s/%s.dsc", dirname, basename);
    FILE* dsc_file = fopen(namebuf, "rb");
    if(dsc_file==NULL)return -1;
    
    int ret = fscanf(dsc_file,"%d%d\n", &block_size, &block_group_size);
    assert(ret==2);
    
    fclose(dsc_file);
    return block_size;
}


static void write_descriptor_file(struct storage__file* c) {
    fprintf(c->description_file, "%d %d\n", 
            c->block_size,
            c->block_group_size);
    fflush(c->description_file);
}

struct storage__file* storage__creat(
            const char* dirname, const char* basename, const char* depname, 
            int block_size, int block_group_size, int best_compression) {
    struct storage__file* c;
    assert(block_size < 32768 && block_size > 0);
    assert(block_group_size > 0 && block_group_size < 32768);
    c = (struct storage__file*)malloc(sizeof(struct storage__file));
    assert(c!=NULL);
    {
        char namebuf[4096];
        snprintf(namebuf, 4096, "%s/%s.dat", dirname, basename);
        c->data_file = fopen(namebuf, "wb+");
        assert(c->data_file != NULL);
        
        snprintf(namebuf, 4096, "%s/%s.idx", dirname, basename);
        c->index_file = fopen(namebuf, "wb+");
        assert(c->index_file != NULL);
        
        snprintf(namebuf, 4096, "%s/%s.dsc", dirname, basename);
        c->description_file = fopen(namebuf, "wb+");
        assert(c->description_file != NULL);
        
        snprintf(namebuf, 4096, "%s/%s.hsh", dirname, basename);
        c->hash_file = fopen(namebuf, "wb+");
        assert(c->hash_file != NULL);
    }
    c->block_size = block_size;
    c->block_group_size = block_group_size;
    c->current_block = 0;
    
    c->current_index_entry = (struct index_entry*)malloc(get_index_entry_size(c));
    memset(c->current_index_entry, 0, get_index_entry_size(c));
    
    write_descriptor_file(c);
    
    c->outbuf = (unsigned char*)malloc(CHUNK);
        
    open_deps(c, dirname, depname);
    
    if(depname) {
        fprintf(c->description_file, "%s", depname);
        fflush(c->description_file);
    }
    
    c->writestat_compressed = 0;
    c->writestat_uncompressible = 0;
    c->writestat_dblrefs = 0;
    c->writestat_reused = 0;
    c->writestat_hashcoll = 0;
    c->writestat_zero = 0;
    
    c->opened_for_writing = 1;
    c->best_compression=best_compression;

    return c;
}


struct storage__file* storage__open(const char* dirname, const char* basename) {
    struct storage__file* c = storage__open_nodeps(dirname, basename);
    
    char namebuf[4096];
    int ret = storage__get_dep_name(dirname, basename, namebuf, 4096);
    if(ret) {
        open_deps(c, dirname, namebuf);
    }
    return c;
}
    
struct storage__file* storage__open_nodeps(const char* dirname, const char* basename) {
    struct storage__file* c;
    c = (struct storage__file*)malloc(sizeof(struct storage__file));
    assert(c!=NULL);
    {
        char namebuf[4096];
        snprintf(namebuf, 4096, "%s/%s.dat", dirname, basename);
        c->data_file = fopen(namebuf, "rb");
        assert(c->data_file != NULL);
        
        snprintf(namebuf, 4096, "%s/%s.idx", dirname, basename);
        c->index_file = fopen(namebuf, "rb");
        assert(c->index_file != NULL);
        
        snprintf(namebuf, 4096, "%s/%s.dsc", dirname, basename);
        c->description_file = fopen(namebuf, "rb");
        assert(c->description_file != NULL);
        
        snprintf(namebuf, 4096, "%s/%s.hsh", dirname, basename);
        c->hash_file = fopen(namebuf, "rb");
        assert(c->hash_file != NULL);
    }
    int ret;
    ret = fscanf(c->description_file, "%d%d", &c->block_size, &c->block_group_size);
    assert(ret==2);
    
    c->current_block = -1;
    
    c->current_index_entry = (struct index_entry*)malloc(get_index_entry_size(c));
    memset(c->current_index_entry, 0, get_index_entry_size(c));
    
    c->outbuf = (unsigned char*)malloc(CHUNK);
    c->depscount = 0;
    
    c->opened_for_writing = 0;
    return c;
}

void storage__flush_index_entry(struct storage__file* c) {
    if (c->current_block == 0) return;
    
    int inside_block_group_offset = (c->current_block-1) % c->block_group_size + 1;
    long long int block_group_offset = (c->current_block-1) / c->block_group_size;
    
    
    //fprintf(stderr, "flush off=%lld cb=%lld bgo=%lld es=%d\n", data_file_offset, c->current_block, block_group_offset, c->block_group_compressed_expected_size);
    
    int len = get_index_entry_size(c);
    
    int ret;
    ret = fseeko(c->index_file, len * block_group_offset, SEEK_SET);
    assert(ret==0);
    
    signed short int *offsets = c->current_index_entry->offsets;
    
    int i;
    for(i=0; i<=inside_block_group_offset; ++i) {
        offsets[i] = htobe16(offsets[i]);
    }
    c->current_index_entry->base_offset = htobe64(c->current_index_entry->base_offset);
    
    ret = fwrite((const char*)c->current_index_entry, 1, len, c->index_file);
    assert(ret == len);
    
    memset(c->current_index_entry, 0, get_index_entry_size(c));
   
    fflush(c->data_file);
    fflush(c->index_file);
}

void storage__close(struct storage__file* c) {
    if (c->opened_for_writing && c->current_block % c->block_group_size != 0) {
        storage__flush_index_entry(c);
    }
    free(c->current_index_entry);
    fclose(c->data_file);
    fclose(c->index_file);
    fclose(c->description_file);
    fclose(c->hash_file);
    free(c);   
}

static void storage__append_block_simple(struct storage__file* c, unsigned char* buf, unsigned char hash) {
    assert(c->opened_for_writing!=0);
    int inside_block_group_offset = c->current_block % c->block_group_size;
    if (inside_block_group_offset==0) {
        off_t data_file_offset = ftello(c->data_file);
        c->current_index_entry->base_offset = data_file_offset;
    }
    
    lzo_uint len = CHUNK;
    if (!c->best_compression) {
        char tmp[LZO1X_1_MEM_COMPRESS];
        lzo1x_1_compress(buf, c->block_size, c->outbuf, &len, &tmp);
    } else {
        char tmp[LZO1X_999_MEM_COMPRESS];
        lzo1x_999_compress(buf, c->block_size, c->outbuf, &len, &tmp);        
    }
    
    if (len >= c->block_size*63/64) {
        // this block looks uncompressible or poorly compressible
        
        int written = fwrite(buf, 1, c->block_size, c->data_file);
        assert(written == c->block_size);
        len = -0x7FFF; // 80 01  on disk
        ++c->writestat_uncompressible;
    } else {
        int written = fwrite(c->outbuf, 1, len, c->data_file);
        assert(written == len);
        ++c->writestat_compressed;
    }
    
    c->current_index_entry->offsets[inside_block_group_offset] = len;
    
    fputc(hash, c->hash_file);
    
    ++c->current_block;
    if (inside_block_group_offset == c->block_group_size-1) {
        storage__flush_index_entry(c);
    }
}

static void storage__append_block_dep(struct storage__file* c, int depnum, unsigned char hash) {
    int inside_block_group_offset = c->current_block % c->block_group_size;
    if (inside_block_group_offset==0) {
        off_t data_file_offset = ftello(c->data_file);
        c->current_index_entry->base_offset = data_file_offset;
    }
    
    c->current_index_entry->offsets[inside_block_group_offset] = -depnum;
    
    fputc(hash, c->hash_file);
    
    ++c->current_block;
    if (inside_block_group_offset == c->block_group_size-1) {
        storage__flush_index_entry(c);
    }
}

void storage__append_block(struct storage__file* c, unsigned char* buf) {
    int i;
    unsigned char hash = phash(buf, c->block_size);
    
    for(i=c->depscount-1; i>=0; --i) {
        unsigned char hc = storage__get_block_hash(c->deps[i], c->current_block);
        if(hc==hash) {
            int ret = storage__read_block_nonrecursive(c->deps[i], c->outbuf, c->current_block);
            if(ret!=0){
                ++c->writestat_dblrefs;
                continue;
            }
            
            if(!memcmp(c->outbuf, buf, c->block_size)) {
                storage__append_block_dep(c, i+1, hash);
                ++c->writestat_reused;
                return;
            } else {
                ++c->writestat_hashcoll;
            }
        }
    }
    
    if(hash==0) {
        // maybe the entire block is zero?
        int j;
        for (j=c->block_size; j>=0; --j) {
            if(buf[j])break;   
        }
        if(j==-1) {
            // the block is zero
            storage__append_block_dep(c, -0x8000, 0);
            ++c->writestat_zero;
            return;
        }
    }

    storage__append_block_simple(c, buf, hash);
}


void storage__get_writestat(const struct storage__file* c
        ,long long int *stat_compressed
        ,long long int *stat_uncompressible
        ,long long int *stat_reused
        ,long long int *stat_hashcoll
        ,long long int *stat_zero
        ,long long int *stat_dblref
    ) {
        if(stat_compressed)     *stat_compressed    = c->writestat_compressed    ;
        if(stat_uncompressible) *stat_uncompressible= c->writestat_uncompressible;
        if(stat_reused)   *stat_reused   = c->writestat_reused  ;
        if(stat_hashcoll) *stat_hashcoll = c->writestat_hashcoll;
        if(stat_zero)     *stat_zero     = c->writestat_zero    ;
        if(stat_dblref)   *stat_dblref   = c->writestat_dblrefs ;
}


void storage__read_block(
            struct storage__file* c, unsigned char* buf, long long int index) {
    int ret = storage__read_block_nonrecursive(c, buf, index);
    if(!ret) return;
    assert(ret<=64);
    --ret;
    
    ret = storage__read_block_nonrecursive(c->deps[ret], buf, index);
    assert(ret==0);
}

int storage__read_block_nonrecursive(
            struct storage__file* c, unsigned char* buf, long long int index) {

    int inside_block_group_offset = index % c->block_group_size;
    long long int block_group_offset = index / c->block_group_size;
    
    signed short int *offsets = c->current_index_entry->offsets;
    int len = get_index_entry_size(c);
    
    if (c->current_block  == -1 || block_group_offset != (c->current_block / c->block_group_size)) {
        int ret;
        ret = fseeko(c->index_file, len * block_group_offset, SEEK_SET);
        assert(ret==0);
        
        ret = fread((char*)c->current_index_entry, 1, len, c->index_file);
        if(ret<len) {
            /* Probably trying to read past the end of file. Feed zero blocks */
            memset(buf, 0, c->block_size);
            return 0;
        }
        
        int i;
        for(i=0; i<c->block_group_size; ++i) {
            offsets[i] = be16toh(offsets[i]);
        }
        c->current_index_entry->base_offset = be64toh(c->current_index_entry->base_offset);
        c->current_block = index;
    }
    
    int i;
    off_t compressed_start = c->current_index_entry->base_offset;
    for(i=0; i<inside_block_group_offset; ++i) {
        if(c->current_index_entry->offsets[i]>0) {
            compressed_start += c->current_index_entry->offsets[i];
        }else
        if(c->current_index_entry->offsets[i]==-0x7FFF) {
            compressed_start += c->block_size;
        }   
    }
    
    len = c->current_index_entry->offsets[inside_block_group_offset];
    //fprintf(stderr, "QQQQ inside_block_group_offset=%04x block_group_offset=%lld len=%04x\n",
    //        inside_block_group_offset, block_group_offset, len);
    if(len==0) {
        // reading past the EOF? feed zeroes
        memset(buf, 0, c->block_size);
        return 0;
    } else
    if(len==-0x8000) {
        memset(buf, 0, c->block_size);
        return 0;        
    }else
    if(len==-0x7FFF){
        // uncompressed block
    }else
    if(len<0) {
        return -len;
    }
    
    int ret;
    ret = fseeko(c->data_file, compressed_start, SEEK_SET);
    assert(ret==0);
    
    if(len!=-0x7FFF) {
        ret = fread(c->outbuf, 1, len, c->data_file);
        assert(ret==len);
        
        lzo_uint decomp_size = c->block_size;
        memset(buf, 0, decomp_size);
        lzo1x_decompress_safe(c->outbuf, len, buf, &decomp_size, NULL);
    } else {
        ret = fread(buf, 1, c->block_size, c->data_file);
        assert(ret==c->block_size);
    }
    
    return 0;
}


unsigned char storage__get_block_hash(struct storage__file* c, long long int i) {
    fseeko(c->hash_file, i, SEEK_SET);
    return fgetc(c->hash_file);
}
