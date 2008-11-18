

include ../Make.rules

all: libsimbus.a

clean:
	rm -f libsimbus.a *.o *~

install: all

L = simbus_pci.o simbus_pci_config.o simbus_pci_rw32.o

libsimbus.a: $L
	rm -f libsimbus.a
	ar cq libsimbus.a $L

simbus_pci.o: simbus_pci.c simbus_pci.h simbus_pci_priv.h
simbus_pci_config.o: simbus_pci_config.c simbus_pci.h simbus_pci_priv.h
simbus_pci_rw32.o: simbus_pci_rw32.c simbus_pci.h simbus_pci_priv.h
