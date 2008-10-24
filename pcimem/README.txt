
The memdev device is not only an example of how to compile and link a
pci device model, it is also a useful device of its own. It implements
a bus-mastering memory that sits on the PCI bus. It can be written to
and read from, it can be configured to do bus-mastered DMA to/from
other PCI devices, and it can be commanded to write its contents to
files.

NOTE: A lot of this code was originally pulled from the pcisim
package, the pci64_memory.v device.
