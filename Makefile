################################################################################
## configuration ###############################################################
################################################################################

CFLAGS = -O2

ifeq (${DEBUG},1)
CFLAGS = -O0 -ggdb -g 
endif

CFLAGS += -Wall -std=c99

INCLUDE = include
SRCPATH = src
TSTPATH = tests
LDFLAGS = -lrt -lm -lpthread -L.
VFLAGS = -D_GNU_SOURCE

UNAME := $(shell uname -n)

CC := cc
CPP := g++

ifeq ($(UNAME), lpdquad)
ifneq ($(TSX), 0)
TSX = 1
endif
endif

ifeq ($(TSX),1)
CFLAGS += -D__TSX__ -mrtm
endif

ifeq ($(UNAME), maglite)
CC = /opt/csw/bin/gcc 
CFLAGS += -m64 -mcpu=v9 -mtune=v9
endif

ifeq ($(UNAME), ol-collab1)
CC = /usr/sfw/bin/gcc
CFLAGS += -m64 -mcpu=v9 -mtune=v9
endif

OS_NAME = $(shell uname -s)

ifeq ($(OS_NAME), Linux)
LDFLAGS += -lnuma
MALLOC += -ljemalloc
endif

ifeq ($(OS_NAME), SunOS)
LDFLAGS += -llgrp
endif


default: mctop
all: mctop mct_load tests
tests: run_on_node0 allocator work_queue work_queue_sort work_queue_sort1 numa_alloc

INCLUDES   := ${INCLUDE}/mctop.h ${INCLUDE}/mctop_mem.h ${INCLUDE}/mctop_profiler.h ${INCLUDE}/helper.h \
	${SRCPATH}/barrier.o ${INCLUDE}/cdf.h ${INCLUDE}/darray.h ${INCLUDE}/mctop_crawler.h

################################################################################
## basic tools #################################################################
################################################################################

FORCE:

MCTOP_OBJS := ${SRCPATH}/mctop.o ${SRCPATH}/mctop_mem.o ${SRCPATH}/mctop_profiler.o ${SRCPATH}/helper.o ${SRCPATH}/numa_sparc.o \
	${SRCPATH}/barrier.o ${SRCPATH}/cdf.o ${SRCPATH}/darray.o ${SRCPATH}/mctop_topology.o ${SRCPATH}/mctop_control.o \
	${SRCPATH}/mctop_aux.o ${SRCPATH}/mctop_load.o ${SRCPATH}/mctop_cache.o

mctop: 	${MCTOP_OBJS} ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${MCTOP_OBJS} -o mctop ${LDFLAGS}

mct_load: ${SRCPATH}/mct_load.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${SRCPATH}/mct_load.o -o mct_load -lmctop ${LDFLAGS}

mctop_latency: ${SRCPATH}/mctop_control.o ${SRCPATH}/mctop_latency.o ${SRCPATH}/helper.o ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${SRCPATH}/mctop_control.o ${SRCPATH}/mctop_latency.o ${SRCPATH}/helper.o -o mctop_latency ${LDFLAGS}


################################################################################
## libmctop.a ##################################################################
################################################################################

MCTOPLIB_OBJS := ${SRCPATH}/cdf.o ${SRCPATH}/darray.o ${SRCPATH}/mctop_aux.o ${SRCPATH}/mctop_topology.o ${SRCPATH}/numa_sparc.o \
	${SRCPATH}/mctop_control.o ${SRCPATH}/mctop_load.o ${SRCPATH}/mctop_graph.o ${SRCPATH}/mctop_alloc.o ${SRCPATH}/mctop_wq.o

libmctop.a: ${MCTOPLIB_OBJS} ${INCLUDES}
	ar cr libmctop.a ${MCTOPLIB_OBJS} ${INCLUDE}/mctop.h

################################################################################
## tests/ | Compiled with libmctop.a and mctop.h from base folder #############
################################################################################

run_on_node0: ${TSTPATH}/run_on_node0.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/run_on_node0.o -o run_on_node0 -lmctop ${LDFLAGS}

allocator: ${TSTPATH}/allocator.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/allocator.o -o allocator -lmctop ${LDFLAGS}

work_queue: ${TSTPATH}/work_queue.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/work_queue.o -o work_queue -lmctop ${LDFLAGS} ${MALLOC}

work_queue_sort: ${TSTPATH}/work_queue_sort.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/work_queue_sort.o -o work_queue_sort -lmctop ${LDFLAGS} ${MALLOC}

work_queue_sort1: ${TSTPATH}/work_queue_sort1.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/work_queue_sort1.o -o work_queue_sort1 -lmctop ${LDFLAGS} ${MALLOC}

sort: ${TSTPATH}/sort.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/sort.o -o sort -lmctop ${LDFLAGS} ${MALLOC}

sort1: ${TSTPATH}/sort1.c libmctop.a ${INCLUDES} ${INCLUDE}/mqsort.h FORCE FORCE
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/sort1.c -o sort1 -lmctop ${LDFLAGS} ${MALLOC} -msse4

sortcc: ${TSTPATH}/sortcc.o libmctop.a ${INCLUDES} 
	${CPP} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/sortcc.o -o sortcc -lmctop ${LDFLAGS} ${MALLOC}

numa_alloc: ${TSTPATH}/numa_alloc.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/numa_alloc.o -o numa_alloc -lmctop ${LDFLAGS}

################################################################################
## .o compilation generic rules ################################################
################################################################################

$(SRCPATH)/%.o:: $(SRCPATH)/%.c 
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} -o $@ -c $<

$(TSTPATH)/%.o:: $(TSTPATH)/%.c 
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} -I./sort_algos/sort -o $@ -c $<


################################################################################
## clean #######################################################################
################################################################################

clean:
	rm src/*.o *.a tests/*.o


################################################################################
## install######################################################################
################################################################################

IPATH := /usr/share/mctop/

install: libmctop.a
	sudo mkdir -p ${IPATH}
	sudo cp desc/* ${IPATH}
	sudo cp libmctop.a /usr/lib/
	sudo cp include/mctop.h /usr/include/

