# mctop
multi-core topology tool

**TODO**: Write a proper README file. 

* Website             : http://lpd.epfl.ch/site/mctop
* Author              : Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
* Related Publications:
  - [*Abstracting Multi-Core Topologies with MCTOP*](http://eurosys2017.org),  
    Georgios Chatzopoulos and Rachid Guerraoui (EPFL), Tim Harris (Oracle Labs), and Vasileios Trigonakis (EPFL/Oracle Labs) (alphabetical order),  
    EuroSys 2017 (*to appear*)

## Compilation
Invoke `make`. `libnuma` is required.

`make install` (requires root priviledges) installs the library and the description of the machine in `/usr/...` under Linux.

## Usage

`mctop` is used to infer the topology of the machine.
Invoke `mctop -h` for details on the parameters that `mctop` accepts.
By default, `mctop` stores the topology of the machine in a `.mct` file, so that applications can later use it.

To load and plot a graph you can use the [./scripts/load_and_plot.sh](./scripts/load_and_plot.sh) script.
If you do not pass any parameters, then it plots topology of the current machine. Otherwise, you can pass the hostname of the target machine (the corresponding mct file should exist in the `desc` folder). This script accepts a second parameter to fix the maximum latency level that you want to print as a direct cross-socket link. For instance, on a 4-socket processor, you might have the following direct links (0,1), (0,2), (1,3), (2,3). In this case, 0 with 3 and 1 with 2 communicate over 2 hops, thus you might want to not represent those links on the graph.

To use the mctop library for scheduling threads (`libmctop.a`), you can simply include `mctop.h` in your software and link with `-lmctop`. For an example, look at [run_on_node0.c](./tests/run_on_node0.c).


