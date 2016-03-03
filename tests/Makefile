CFLAGS = -O2

ifeq (${DEBUG},1)
CFLAGS = -O0 -ggdb
endif

CFLAGS += -Wall -std=c99

LDFLAGS = -lmctop -lrt -lm -pthread -lnuma 

 $(info !! Using as the libmctop.a and mctop.h installed in /usr/..)

default: run_on_node0

all: 

run_on_node0: run_on_node0.o
	cc $(CFLAGS) $(VFLAGS) run_on_node0.o -o run_on_node0 ${LDFLAGS}

clean:
	rm *.o

%.o:: %.c 
	cc $(CFLAGS) $(VFLAGS) -I${INCLUDES} -o $@ -c $<


