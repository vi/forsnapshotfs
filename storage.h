struct storage__file;

size_t storage__get_block_size(const struct storage__file* c) ;

size_t storage__get_block_size2(const char* dirname, const char* basename) ;

// for writing only
struct storage__file* storage__creat(
            const char* dirname, const char* basename, const char* depname,
            int block_size, int block_group_size, int best_compression) ;
    
// for reading only
struct storage__file* storage__open(
            const char* dirname, const char* basename);

void storage__close(struct storage__file* c);

void storage__append_block(struct storage__file* c, unsigned char* buf) ;

void storage__read_block(struct storage__file* c, unsigned char* buf, long long int i);
    
// returns 0 on success, N of need to read from N'th dependency
int storage__read_block_nonrecursive(struct storage__file* c, unsigned char* buf, long long int i);
    
// only when reading
unsigned char storage__get_block_hash(struct storage__file* c, long long int i);
    
long long int storage__get_number_of_blocks(const struct storage__file* c);

long long int storage__get_number_of_blocks2(const char* dirname, const char* basename) ;
    
void storage__get_writestat(const struct storage__file* c
        ,long long int *stat_new
        ,long long int *stat_reused
        ,long long int *stat_hashcoll
        ,long long int *stat_zero
        ,long long int *stat_dblref
    );
