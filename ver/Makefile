
include ../Make.rules

all:

clean:
	rm -f *~

install: all installdirs $(datadir)/pci_slot.v

uninstall:
	rm -f $(DESTDIR)$(datadir)/pci_slot.v


$(datadir)/pci_slot.v: pci_slot.v
	$(INSTALL_DATA) pci_slot.v $(DESTDIR)$(datadir)/pci_slot.v

installdirs: ../mkinstalldirs
	$(srcdir)/../mkinstalldirs $(DESTDIR)$(datadir)
