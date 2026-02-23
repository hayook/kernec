
SRC = kernec.c process.c queue.c

all: kernec

kernec: ${SRC} kernec.h process.h queue.h
	gcc ${SRC} -o $@

run: kernec
	./kernec
