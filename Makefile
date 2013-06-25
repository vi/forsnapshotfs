all: write read verify

CFLAGS=-O2 -ggdb -Wall

STORAGE = storage.o hash.o

write: storage.o hash.o write.c
		${CC} ${LDFALGS} ${CFLAGS} -llzo2 ${STORAGE} write.c -o write
		
read: storage.o hash.o read.c
		${CC} ${LDFALGS} ${CFLAGS} -llzo2 ${STORAGE} read.c -o read
		
verify: storage.o hash.o verify.c
		${CC} ${LDFALGS} ${CFLAGS} -llzo2 ${STORAGE} verify.c -o verify

storage.o: storage.c hash.h
		${CC} ${CFLAGS} -c storage.c
		
hash.o: hash.c
		${CC} ${CFLAGS} -funroll-loops -c hash.c
