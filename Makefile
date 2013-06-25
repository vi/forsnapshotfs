all: fsfs-write fsfs-read fsfs-verify fsfs-mount

CFLAGS=-O2 -ggdb -Wall

STORAGE = storage.o hash.o

fsfs-write: ${STORAGE} write.c
		${CC} ${LDFALGS} ${CFLAGS} -llzo2 ${STORAGE} write.c -o fsfs-write
		
fsfs-read: ${STORAGE} read.c
		${CC} ${LDFALGS} ${CFLAGS} -llzo2 ${STORAGE} read.c -o fsfs-read
		
fsfs-verify: ${STORAGE} verify.c
		${CC} ${LDFALGS} ${CFLAGS} -llzo2 ${STORAGE} verify.c -o fsfs-verify
		
fsfs-mount: ${STORAGE} mount.c
		${CC} ${LDFLAGS} ${CFLAGS} -llzo2 ${STORAGE} $(shell pkg-config fuse --cflags --libs) mount.c -o fsfs-mount

storage.o: storage.c hash.h
		${CC} ${CFLAGS} -c storage.c
		
hash.o: hash.c
		${CC} ${CFLAGS} -funroll-loops -c hash.c
