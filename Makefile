CFLAGS = -O3

ifeq (${DEBUG},1)
CFLAGS = -O0 -ggdb
endif

CFLAGS += -Wall -std=c99

INCLUDE = include
SRCPATH = src
LDFLAGS = -lrt -lm -pthread -lnuma -L.
VFLAGS = -D_GNU_SOURCE

default: mctop

all: mctop mctop_latency

MCTOP_OBJS := ${SRCPATH}/mctop.o ${SRCPATH}/mctop_mem.o ${SRCPATH}/helper.o ${SRCPATH}/barrier.o ${SRCPATH}/pfd.o ${SRCPATH}/cdf.o \
	${SRCPATH}/darray.o ${SRCPATH}/mctop_topology.o ${SRCPATH}/mctop_control.o
INCLUDES   := ${INCLUDE}/mctop.h ${INCLUDE}/mctop_mem.h ${INCLUDE}/helper.h ${SRCPATH}/barrier.o ${INCLUDE}/pfd.h ${INCLUDE}/cdf.h \
	${INCLUDE}/darray.h ${INCLUDE}/mctop_crawler.h

mctop: 	${MCTOP_OBJS} ${INCLUDES}
	cc $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${MCTOP_OBJS} -o mctop ${LDFLAGS}

mctop_latency: ${SRCPATH}/mctop_latency.o ${SRCPATH}/helper.o ${INCLUDES}
	cc $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${SRCPATH}/mctop_latency.o ${SRCPATH}/helper.o -o mctop_latency ${LDFLAGS}

clean:
	rm src/*.o

$(SRCPATH)/%.o:: $(SRCPATH)/%.c 
	cc $(CFLAGS) $(VFLAGS) -I${INCLUDE} -o $@ -c $<

