#include <malloc.h>
#include <assert.h>
#include <string.h>

#include "simplecow.h"


struct extent {
    long long int start;
    int size;
    char* buf;
};

// just array of all write operations
struct simplecow {
    struct extent* extents;
    int extent_count;
    int extent_capacity;
    backing_read_t backing_read;
    void *backing_read_usr;
};


int simplecow_write(struct simplecow *cow, long long int offset, int size, char* buf) {
    if(cow->extent_count == cow->extent_capacity || cow->extents == NULL) {
        cow->extent_capacity*=2;
        cow->extents = realloc(cow->extents, cow->extent_capacity*sizeof(struct extent));
        assert(cow->extents != NULL);
    }
    
    char* newbuf = (char*)malloc(size);
    assert(newbuf!=NULL);
    memcpy(newbuf, buf, size);
    struct extent* ex = &cow->extents[cow->extent_count++];
    ex->buf = newbuf;
    ex->start = offset;
    ex->size = size;
    return size;
}

int simplecow_read1(struct simplecow *cow, long long int offset, int size, char* buf) {
    int i;
    
    struct extent* e = cow->extents;
    
    int how_much_to_read_from_backing_store_new = size;
    for(i=cow->extent_count-1; i>=0; --i) {
        if(e[i].start + e[i].size <= offset || offset + how_much_to_read_from_backing_store_new < e[i].start) {
            // this extent does not intersect with our planned read   
        } else {
            int new_limit = e[i].start - offset;
            if(new_limit >= 0 && new_limit < how_much_to_read_from_backing_store_new) {
                how_much_to_read_from_backing_store_new = new_limit;
            }
            
            if(e[i].start <= offset && e[i].start + e[i].size > offset) {
                // in middle of some extent
                how_much_to_read_from_backing_store_new = 0;
            }
        }
    }
    
    if(how_much_to_read_from_backing_store_new>0) {
        // read something from backing store
        //fprintf(stderr, "Reading from backing store limit %d\n", how_much_to_read_from_backing_store_new);
        return (*cow->backing_read)(cow->backing_read_usr, offset, how_much_to_read_from_backing_store_new, buf);
    }
    
    // There is some extent we should copy from
    
    struct extent *active_extent;
    int offset_inside_active_extent;
    for(i=cow->extent_count-1; i>=0; --i) {
        if(e[i].start > offset) continue;
        if(e[i].start <= offset && e[i].start + e[i].size > offset) {
            offset_inside_active_extent = offset - e[i].start;
            break;
        }
    }
    assert(i>=0);
    active_extent=&e[i];
    
    int limit = size;
    if(active_extent->size-offset_inside_active_extent  <  size) {
        limit = active_extent->size - offset_inside_active_extent;
    }
    
    // Now check if other, smaller, but newer extents interfere with our active_extent
    int j;
    for(j=cow->extent_count-1; j>i; --j) {
        //fprintf(stderr, "  checking if extent %d limits ourselves\n", j);
        if(e[j].start >= offset && e[j].start < offset+limit) {
            limit = e[j].start-offset;   
            //fprintf(stderr, "  limit reduced to %d\n", limit);
        }
    }
    assert(limit>0);
    
    //fprintf(stderr, "Reading from extent %d at offset %d limit %d\n", active_extent - cow->extents, offset_inside_active_extent, limit);
    memcpy(buf, active_extent->buf+offset_inside_active_extent, limit);
    return limit;
}


int simplecow_read(struct simplecow *cow, long long int offset, int size, char* buf) {
    while(size>0) {
        int ret = simplecow_read1(cow, offset, size, buf);
        buf+=ret;
        offset+=ret;
        size-=ret;
    }
    return size;
}

struct simplecow* simplecow_create(backing_read_t br, void* usr) {
    struct simplecow *cow = (struct simplecow*)malloc(sizeof(*cow));
    cow->extent_count = 0;
    cow->extent_capacity = 128;
    cow->extents = NULL;
    cow->backing_read = br;
    cow->backing_read_usr = usr;
    return cow;
}
void simplecow_destroy(struct simplecow* cow) {
    int i;
    if(cow->extents) {
        for(i=0; i < cow->extent_count; ++i) {
            free(cow->extents[i].buf);   
        }
        free(cow->extents);
    }
    free(cow);
}
