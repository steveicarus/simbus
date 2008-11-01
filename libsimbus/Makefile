

L = simbus_pci.o

libsimbus.a: $L
	rm -f libsimbus.a
	ar cq libsimbus.a $L

simbus_pci.o: simbus_pci.c simbus_pci.h
