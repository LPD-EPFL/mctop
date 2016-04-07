################################################################################
## configuration ###############################################################
################################################################################

CFLAGS = -O2
CPPFLAGS = -O2

ifeq (${DEBUG},1)
CFLAGS = -O0 -ggdb -g 
CPPFLAGS = -O0 -ggdb -g 
endif

CFLAGS += -Wall -std=c99 -msse4
CPPFLAGS += -Wall -msse4

INCLUDE = include
SRCPATH = src
TSTPATH = tests
MSTPATH = tests/merge_sort/
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
	${SRCPATH}/mctop_control.o ${SRCPATH}/mctop_load.o ${SRCPATH}/mctop_graph.o ${SRCPATH}/mctop_alloc.o ${SRCPATH}/mctop_wq.o \
	${SRCPATH}/mctop_node_tree.o

libmctop.a: ${MCTOPLIB_OBJS} ${INCLUDES}
	ar cr libmctop.a ${MCTOPLIB_OBJS} ${INCLUDE}/mctop.h

################################################################################
## tests/ | Compiled with libmctop.a and mctop.h from base folder #############
################################################################################

tests: run_on_node0 allocator node_tree work_queue work_queue_sort work_queue_sort1 sort sort1 sortcc \
	 numa_alloc mergesort

mergesort: merge_sort_std merge_sort_std_parallel merge_sort_parallel_merge \
	merge_sort_parallel_merge_nosse merge_sort_seq_merge

run_on_node0: ${TSTPATH}/run_on_node0.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/run_on_node0.o -o run_on_node0 -lmctop ${LDFLAGS}

allocator: ${TSTPATH}/allocator.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/allocator.o -o allocator -lmctop ${LDFLAGS}

node_tree: ${TSTPATH}/node_tree.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/node_tree.o -o node_tree -lmctop ${LDFLAGS}

work_queue: ${TSTPATH}/work_queue.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/work_queue.o -o work_queue -lmctop ${LDFLAGS} ${MALLOC}

work_queue_sort: ${TSTPATH}/work_queue_sort.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/work_queue_sort.o -o work_queue_sort -lmctop ${LDFLAGS} ${MALLOC}

work_queue_sort1: ${TSTPATH}/work_queue_sort1.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/work_queue_sort1.o -o work_queue_sort1 -lmctop ${LDFLAGS} ${MALLOC}

sort: ${TSTPATH}/sort.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/sort.o -o sort -lmctop ${LDFLAGS} ${MALLOC}

mctop_sort: ${TSTPATH}/mctop_sort.o ${MSTPATH}/mctop_sort.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${MSTPATH}/mctop_sort.o ${TSTPATH}/mctop_sort.o -o mctop_sort -lmctop ${LDFLAGS} ${MALLOC}

sort1: ${TSTPATH}/sort1.c libmctop.a ${INCLUDES} ${INCLUDE}/mqsort.h FORCE FORCE
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/sort1.c -o sort1 -lmctop ${LDFLAGS} ${MALLOC}

sortcc: libmctop.a ${INCLUDES} 
	${CPP} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/sortcc.cc -o sortcc -lmctop ${LDFLAGS} ${MALLOC}

numa_alloc: ${TSTPATH}/numa_alloc.o libmctop.a ${INCLUDES}
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/numa_alloc.o -o numa_alloc -lmctop ${LDFLAGS}

merge_sort_std: ${TSTPATH}/merge_sort/merge_sort_std.cpp libmctop.a ${INCLUDES}
	${CPP} $(CPPFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/merge_sort/merge_sort_std.cpp -o merge_sort_std -lmctop ${LDFLAGS} -ljemalloc

merge_sort_std_parallel: ${TSTPATH}/merge_sort/merge_sort_std_parallel.cpp libmctop.a ${INCLUDES}
	${CPP} $(CPPFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/merge_sort/merge_sort_std_parallel.cpp -o merge_sort_std_parallel -lmctop ${LDFLAGS} -ljemalloc -fopenmp

merge_sort_parallel_merge: ${TSTPATH}/merge_sort/merge_sort_parallel_merge.cpp libmctop.a ${INCLUDES} FORCE FORCE
	${CPP} $(CPPFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/merge_sort/merge_sort_parallel_merge.cpp -o merge_sort_parallel_merge -lmctop ${LDFLAGS} -ljemalloc -fopenmp

merge_sort_parallel_merge_imbalanced: ${TSTPATH}/merge_sort/merge_sort_parallel_merge_imbalanced.cpp libmctop.a ${INCLUDES} FORCE FORCE
	${CPP} $(CPPFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/merge_sort/merge_sort_parallel_merge_imbalanced.cpp -o merge_sort_parallel_merge_imbalanced -lmctop ${LDFLAGS} -ljemalloc -fopenmp

merge_sort_parallel_merge_notworking: ${TSTPATH}/merge_sort/merge_sort_parallel_merge_notworking.cpp libmctop.a ${INCLUDES} FORCE FORCE
	${CPP} $(CPPFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/merge_sort/merge_sort_parallel_merge_notworking.cpp -o merge_sort_parallel_merge_notworking -lmctop ${LDFLAGS} -ljemalloc -fopenmp

merge_sort_merge_level: ${TSTPATH}/merge_sort/merge_sort_merge_level.cpp libmctop.a ${INCLUDES} FORCE FORCE
	${CPP} $(CPPFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/merge_sort/merge_sort_merge_level.cpp -o merge_sort_merge_level -lmctop ${LDFLAGS} -ljemalloc -fopenmp

merge_sort_merge_level_up: ${TSTPATH}/merge_sort/merge_sort_merge_level_up.cpp libmctop.a ${INCLUDES} FORCE FORCE
	${CPP} $(CPPFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/merge_sort/merge_sort_merge_level_up.cpp -o merge_sort_merge_level_up -lmctop ${LDFLAGS} -ljemalloc -fopenmp

merge_sort_parallel_merge_nosse: ${TSTPATH}/merge_sort/merge_sort_parallel_merge_nosse.cpp libmctop.a ${INCLUDES}
	${CPP} $(CPPFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/merge_sort/merge_sort_parallel_merge_nosse.cpp -o merge_sort_parallel_merge_nosse -lmctop ${LDFLAGS} -ljemalloc -fopenmp

merge_sort_seq_merge: ${TSTPATH}/merge_sort/merge_sort_seq_merge.cpp libmctop.a ${INCLUDES}
	${CPP} $(CPPFLAGS) $(VFLAGS) -I${INCLUDE} ${TSTPATH}/merge_sort/merge_sort_seq_merge.cpp -o merge_sort_seq_merge -lmctop ${LDFLAGS} -ljemalloc -fopenmp

################################################################################
## .o compilation generic rules ################################################
################################################################################

$(SRCPATH)/%.o:: $(SRCPATH)/%.c 
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} -o $@ -c $<

$(MSTPATH)/%.o:: $(MSTPATH)/%.cc
	${CPP} $(CFLAGS) $(VFLAGS) -I${INCLUDE} -o $@ -c $<

$(TSTPATH)/%.o:: $(TSTPATH)/%.c 
	${CC} $(CFLAGS) $(VFLAGS) -I${INCLUDE} -o $@ -c $<


################################################################################
## clean #######################################################################
################################################################################

clean:
	rm -f src/*.o *.a tests/*.o tests/merge_sort/*.o


################################################################################
## install######################################################################
################################################################################

IPATH := /usr/share/mctop/

install: libmctop.a
	sudo mkdir -p ${IPATH}
	sudo cp desc/* ${IPATH}
	sudo cp libmctop.a /usr/lib/
	sudo cp include/mctop.h /usr/include/
	sudo cp include/mctop_alloc.h /usr/include/
