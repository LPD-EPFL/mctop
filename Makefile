CFLAGS = -O3

ifeq (${DEBUG},1)
CFLAGS = -O0 -ggdb
endif

CFLAGS += -Wall -std=c99

INCLUDE = include
SRCPATH = src
LDFLAGS = -lrt -lm -pthread -L.
VFLAGS = -D_GNU_SOURCE

default: mctop

MCTOP_OBJS := ${SRCPATH}/mctop.o ${SRCPATH}/helper.o ${SRCPATH}/pfd.o ${SRCPATH}/cdf.o ${SRCPATH}/darray.o

mctop: 	${MCTOP_OBJS} ${INCLUDE}/barrier.h ${INCLUDE}/helper.h
	cc $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${MCTOP_OBJS} -o mctop ${LDFLAGS}

clean:
	rm src/*.o

$(SRCPATH)/%.o:: $(SRCPATH)/%.c 
	cc $(CFLAGS) $(VFLAGS) -I${INCLUDE} -o $@ -c $<

