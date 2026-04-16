
SRC = kernec.c process.c queue.c vector.c klua.c

all: kernec

kernec: ${SRC} kernec.h process.h queue.h vector.h klua.h
	gcc ${SRC} -o $@ -llua5.3

run: kernec
	./kernec
