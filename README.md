# mctop
multi-core topology tool

* Website             : http://lpd.epfl.ch/site/mctop
* Author              : Vasileios Trigonakis <github@trigonakis.com>
* Related Publications:
  - [*Abstracting Multi-Core Topologies with MCTOP*](http://eurosys2017.org),  
    Georgios Chatzopoulos and Rachid Guerraoui (EPFL), Tim Harris (Oracle Labs), and Vasileios Trigonakis (EPFL/Oracle Labs) (alphabetical order),  
    EuroSys 2017 (*to appear*)

## Compilation
Invoke `make`. `libnuma` is required.

`make install` (requires root priviledges) installs the library and the description of the machine in `/usr/...` under Linux.

## Usage

#### Using mctop to harvest the topology of a machine

`mctop` is used to infer the topology of the machine.
Invoke `mctop -h` for details on the parameters that `mctop` accepts.
By default, `mctop` stores the topology of the machine in a `.mct` file, so that applications can later use it.

To load and plot a graph you can use the [./scripts/load_and_plot.sh](./scripts/load_and_plot.sh) script.
If you do not pass any parameters, then it plots topology of the current machine. Otherwise, you can pass the hostname of the target machine (the corresponding mct file should exist in the `desc` folder). This script accepts a second parameter to fix the maximum latency level that you want to print as a direct cross-socket link. For instance, on a 4-socket processor, you might have the following direct links (0,1), (0,2), (1,3), (2,3). In this case, 0 with 3 and 1 with 2 communicate over 2 hops, thus you might want to not represent those links on the graph.

#### Using libmctop in your application

To use the mctop library for scheduling threads (`libmctop.a`), you can simply include `mctop.h` in your software and link with `-lmctop`.




###### The data structures 

In `mctop.h` you can find the data structures that describe the topology of a machine. The main data structures are:

* struct hw_context: describes a hardware context, including an id, the type of the context (whether it is a core or a hardware context), pointers to the socket the context belongs to and links to other contexts of the same level.
* struct hwc_gs_t: describes a group of hardware contexts: this can either be a core with multiple hardware contexts, a socket, or a grouping of cores/sockets that have the same latency in communication. This struct includes an id of its own, information on the number and pointers to the "children" that belong to this group (e.g. hardware contexts of a core, cores of a socket, etc.), latency and bandwidth information within the group, as well as towards other groups at the same level, as well as latencies and bandwidth towards all memory nodes.
* struct mctop: describes a topology, including information on the number of levels included, latencies per level, numbers of hardware sockets / sockets / hardware contexts per core, as well as pointers to the next levels.

###### Querying the topology / Thread and memory placement

Inside the header file you will find the API for querying the topology (see `topo getters`, `socket getters`, and below), as well as the API for running on specific nodes/sockets (see `MCTOP Scheduling`). Inuding the `mctop_alloc.h` will enable you to use the policies we have created on top of mctop (see `mctop_alloc_policy`), as well as various functions for pininng threads (see `mctop_alloc_pin`), getting the core/context that the current thread is running on (see `mctop_alloc_thread_id`/`mctop_alloc_tread_incore_id`), as well as the merge tree we used in mctop_sort (see `Node merge tree`).

#### Examples and applications

For examples on how to use the API, look at [run_on_node0.c](./tests/run_on_node0.c), [sort.c](./tests/sort.c) and [node_tree.c](./tests/node_tree.c).

## DVFS

Note that DVFS is the worst enemy of `mctop`. In case `mctop` fails to infer the topology of a processor, even after tuning its parameters, you can try disabling DVFS from the BIOS settings of the processor.


