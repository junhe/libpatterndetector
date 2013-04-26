# Define required macros here
SHELL = /bin/sh

OBJS=example.o pattern.o 
CFLAGS=-Wall -D_FILE_OFFSET_BITS=64
CC=g++
INCLUDES=
LIBS=

xexample:${OBJS}
	${CC} ${CFLAGS} ${INCLUDES}  ${OBJS} -o $@ ${LIBS}

clean:
	-rm -f *.o core *.core *.gch

.cpp.o:
	${CC} ${CFLAGS} ${INCLUDES} -c $<
