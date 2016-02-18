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

MCTOP_OBJS := ${SRCPATH}/mctop.o ${SRCPATH}/helper.o ${SRCPATH}/barrier.o ${SRCPATH}/pfd.o ${SRCPATH}/cdf.o ${SRCPATH}/darray.o \
		${SRCPATH}/mctop_topology.o
INCLUDES   := ${INCLUDE}/mctop.h ${INCLUDE}/helper.h ${SRCPATH}/barrier.o ${INCLUDE}/pfd.h ${INCLUDE}/cdf.h ${INCLUDE}/darray.h \
		${INCLUDE}/mctop_crawler.h

mctop: 	${MCTOP_OBJS} ${INCLUDES}
	cc $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${MCTOP_OBJS} -o mctop ${LDFLAGS}

clean:
	rm src/*.o

$(SRCPATH)/%.o:: $(SRCPATH)/%.c 
	cc $(CFLAGS) $(VFLAGS) -I${INCLUDE} -o $@ -c $<

