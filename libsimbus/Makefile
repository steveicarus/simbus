

include ../Make.rules

all: libsimbus.a

clean:
	rm -f libsimbus.a *.o *~

install: all installdirs $(libdir)/libsimbus.a $(includedir)/simbus_pci.h

uninstall:
	rm -f $(DESTDIR)$(libdir)/libsimbus.a
	rm -f $(DESTDIR)$(includedir)/simbus_pci.h

L = simbus_pci.o simbus_pci_config.o simbus_pci_rw32.o

libsimbus.a: $L
	rm -f libsimbus.a
	ar cq libsimbus.a $L

simbus_pci.o: simbus_pci.c simbus_pci.h simbus_pci_priv.h
simbus_pci_config.o: simbus_pci_config.c simbus_pci.h simbus_pci_priv.h
simbus_pci_rw32.o: simbus_pci_rw32.c simbus_pci.h simbus_pci_priv.h

$(libdir)/libsimbus.a: libsimbus.a
	$(INSTALL_DATA) libsimbus.a $(DESTDIR)$(libdir)/libsimbus.a

$(includedir)/simbus_pci.h: simbus_pci.h
	$(INSTALL_DATA) simbus_pci.h $(DESTDIR)$(includedir)/simbus_pci.h


installdirs: ../mkinstalldirs
	$(srcdir)/../mkinstalldirs $(DESTDIR)$(libdir) $(DESTDIR)$(includedir)
