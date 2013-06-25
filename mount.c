
#define FUSE_USE_VERSION 26

#define _GNU_SOURCE

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <semaphore.h>

#include "storage.h"

static int fsfs_getattr(const char *path, struct stat *stbuf)
{
	if(!path) return -ENOSYS;
    if(!strcmp(path,"/")) {
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 1;
        return 0;   
    }
    
    int ret1, ret2, ret3;
    char namebuf[4096];
	
    snprintf(namebuf, 4096, "%s.idx", path+1);
    ret1 = stat(namebuf, stbuf);
    snprintf(namebuf, 4096, "%s.dsc", path+1);
    ret2 = stat(namebuf, stbuf);
    snprintf(namebuf, 4096, "%s.dat", path+1);
    ret3 = stat(namebuf, stbuf);
	
	if(ret1 || ret2 || ret3) {
	   return -errno;   
	}
	
    long long int q = storage__get_number_of_blocks2(".",path+1);
    size_t w = storage__get_block_size2(".", path+1);
    stbuf->st_size = q * w;
    stbuf->st_blocks = stbuf->st_size / 512;
	return 0;
}


static int fsfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi) {
    
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	dp = opendir(".");
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		
		size_t l = strlen(de->d_name);
		if(!strncasecmp(de->d_name+l-4, ".dsc", 4)) {
		    de->d_name[l-4]=0; // trim .dsc
    		if (filler(buf, de->d_name, &st, 0))
    			break;
		}
	}

	closedir(dp);
	return 0;
}

struct myinfo {
    struct storage__file* f;
    sem_t sem;
    unsigned char *tmpbuf;
};

static int fsfs_open(const char *path, struct fuse_file_info *fi)
{
    struct stat stbuf;
    int ret = fsfs_getattr(path, &stbuf);
    if(ret) return ret;
    
    const char* basename = path+1;
    const char* dirname = "."; // use current directory
    
    struct myinfo* i = (struct myinfo*)malloc(sizeof *i);
    i->f = storage__open(dirname, basename);
    sem_init(&i->sem, 0, 1);
    
	int bs = storage__get_block_size(i->f);
	
	i->tmpbuf=(unsigned char*)malloc(bs);
    
    fi->fh = (uintptr_t)  i;
    
    return 0;
}

static int fsfs_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	struct myinfo* i = (struct myinfo*)(uintptr_t)  fi->fh;
	if(!i) return -ENOSYS;
	
    sem_wait(&i->sem);
	
	int bs = storage__get_block_size(i->f);
	
	int counter=0;
	for(;size>0;) {
    	long long int bn = offset / bs;
    	int off = offset % bs;
    		
    	//fprintf(stderr, "bn=%lld off=%d counter=%d size=%d offset=%lld\n",
    	//       bn, off, counter, size, offset);
    	
	    storage__read_block(i->f, i->tmpbuf, bn);
	    
	    int size_to_copy = bs-off;
	    if (size_to_copy>size) size_to_copy = size;
	    
	    memcpy(buf+counter, i->tmpbuf+off, size_to_copy);
	    
	    size-=size_to_copy;
	    offset+=size_to_copy;
	    counter+=size_to_copy;
	}
    sem_post(&i->sem);
	return counter;
	
    /*int res;

	(void) path;
	res = pread(fi->fh, buf, size, offset);
	if (res == -1)
		res = -errno;

	return res;*/
}

static int fsfs_release(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	
	struct myinfo* i = (struct myinfo*)(uintptr_t)  fi->fh;
	if(!i) return -ENOSYS;
	
    storage__close(i->f);
	sem_destroy(&i->sem);
	free(i->tmpbuf);

	return 0;
}


static struct fuse_operations fsfs_oper = {
	.getattr	= fsfs_getattr,
	.readdir	= fsfs_readdir,
	.open		= fsfs_open,
	.read		= fsfs_read,
	.release	= fsfs_release,

	.flag_nullpath_ok = 1,
};

int main(int argc, char *argv[])
{
	umask(0);
	return fuse_main(argc, argv, &fsfs_oper, NULL);
}
