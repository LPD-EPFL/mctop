# mctop
multi-core topology tool

## Compilation
Invoke `make`. `libnuma` is required.

`make install` (requires root priviledges) installs the library and the description of the machine in `/usr/...` under Linux.

## Usage

`mctop` is used to infer the topology of the machine.
Invoke `mctop -h` for details on the parameters that `mctop` accepts.
By default, `mctop` stores the topology of the machine in a `.mct` file, so that applications can later use it.

To load and plot a graph you can use the [./scripts/load_and_plot.sh](./scripts/load_and_plot.sh) script.
If you do not pass any parameters, then it plots topology of the current machine. Otherwise, you can pass the hostname of the target machine (the corresponding mct file should exist in the `desc` folder).

To use the mctop library for scheduling threads (`libmctop.a`), you can simply include `mctop.h` in your software and link with `-lmctop`. For an example, look at [run_on_node0.c](./tests/run_on_node0.c).

## Interface

For now, I have implemented very few scheduling functions in `libmctop`. 
I will extend the interface as needed.
