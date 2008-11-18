
The simbus project is a toolkit for simulating PCI-based systems such
that each PCI device can be its own simulation process, anythere on
a network. This allows for distributed simulation of PCI systems.

If you are getting this source code from a git repository, then you
must first "autoconf" in the root of the source tree to generate the
configure script.

PREREQUISITES

To build: iverilog 0.9 (or development snapshots), flex, bison,
          a C++ compiler and a C compiler.

To run: iverilog 0.9 (or development snapshots)
        a C compiler if you plan on using the libsimbus*.a libraries.

CONFIGURE

   ./configure

COMPILE

   make all

INSTALL

   make install
