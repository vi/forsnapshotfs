

typedef int (*backing_read_t)(void* usr, long long int offset, int size, char* buf);

struct simplecow;



int simplecow_write(struct simplecow *cow, long long int offset, int size, char* buf) ;


int simplecow_read(struct simplecow *cow, long long int offset, int size, char* buf) ;


struct simplecow* simplecow_create(backing_read_t br, void* usr) ;


void simplecow_destroy(struct simplecow* cow) ;
