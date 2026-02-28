
SRC = kernec.c process.c queue.c vector.c

all: kernec

kernec: ${SRC} kernec.h process.h queue.h vector.h
	gcc ${SRC} -o $@

run: kernec
	./kernec
