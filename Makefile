CFLAGS = -O2

ifeq (${DEBUG},1)
CFLAGS = -O0 -ggdb
endif

CFLAGS += -Wall -std=c99

INCLUDE = include
SRCPATH = src
LDFLAGS = -lrt -lm -pthread -lnuma -L.
VFLAGS = -D_GNU_SOURCE

default: mctop

all: mctop mct_load mctop_latency

MCTOP_OBJS := ${SRCPATH}/mctop.o ${SRCPATH}/mctop_mem.o ${SRCPATH}/mctop_profiler.o ${SRCPATH}/helper.o \
	${SRCPATH}/barrier.o ${SRCPATH}/cdf.o ${SRCPATH}/darray.o ${SRCPATH}/mctop_topology.o ${SRCPATH}/mctop_control.o \
	${SRCPATH}/mctop_aux.o ${SRCPATH}/mctop_load.o 
INCLUDES   := ${INCLUDE}/mctop.h ${INCLUDE}/mctop_mem.h ${INCLUDE}/mctop_profiler.h ${INCLUDE}/helper.h \
	${SRCPATH}/barrier.o ${INCLUDE}/cdf.h ${INCLUDE}/darray.h ${INCLUDE}/mctop_crawler.h

mctop: 	${MCTOP_OBJS} ${INCLUDES}
	cc $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${MCTOP_OBJS} -o mctop ${LDFLAGS}

mctop_latency: ${SRCPATH}/mctop_latency.o ${SRCPATH}/helper.o ${INCLUDES}
	cc $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${SRCPATH}/mctop_latency.o ${SRCPATH}/helper.o -o mctop_latency ${LDFLAGS}

MCTOPLIB_OBJS := ${SRCPATH}/cdf.o ${SRCPATH}/darray.o ${SRCPATH}/mctop_aux.o \
	${SRCPATH}/mctop_topology.o ${SRCPATH}/mctop_control.o ${SRCPATH}/mctop_load.o

libmctop.a: ${MCTOPLIB_OBJS} ${INCLUDES}
	ar cr libmctop.a ${MCTOPLIB_OBJS} ${INCLUDE}/mctop.h

mct_load: ${SRCPATH}/mct_load.o libmctop.a ${INCLUDES}
	cc $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${SRCPATH}/mct_load.o -o mct_load -lmctop ${LDFLAGS}
clean:
	rm src/*.o *.a

$(SRCPATH)/%.o:: $(SRCPATH)/%.c 
	cc $(CFLAGS) $(VFLAGS) -I${INCLUDE} -o $@ -c $<


IPATH := /usr/share/mctop/

install: libmctop.a
	sudo mkdir -p ${IPATH}
	sudo cp desc/* ${IPATH}
	sudo cp libmctop.a /usr/lib/
	sudo cp include/mctop.h /usr/include/
