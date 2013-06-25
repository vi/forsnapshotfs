all: fsfs-write fsfs-read fsfs-verify

CFLAGS=-O2 -ggdb -Wall

STORAGE = storage.o hash.o

fsfs-write: storage.o hash.o write.c
		${CC} ${LDFALGS} ${CFLAGS} -llzo2 ${STORAGE} write.c -o fsfs-write
		
fsfs-read: storage.o hash.o read.c
		${CC} ${LDFALGS} ${CFLAGS} -llzo2 ${STORAGE} read.c -o fsfs-read
		
fsfs-verify: storage.o hash.o verify.c
		${CC} ${LDFALGS} ${CFLAGS} -llzo2 ${STORAGE} verify.c -o fsfs-verify

storage.o: storage.c hash.h
		${CC} ${CFLAGS} -c storage.c
		
hash.o: hash.c
		${CC} ${CFLAGS} -funroll-loops -c hash.c
