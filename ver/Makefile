
include ../Make.rules

all:

clean:
	rm -f *~

install: all installdirs $(datadir)/pci_slot.v $(datadir)/point_slave_io.v

uninstall:
	rm -f $(DESTDIR)$(datadir)/pci_slot.v
	rm -f $(DESTDIR)$(datadir)/point_slave_io.v


$(datadir)/pci_slot.v: pci_slot.v
	$(INSTALL_DATA) pci_slot.v $(DESTDIR)$(datadir)/pci_slot.v

$(datadir)/point_slave_io.v: point_slave_io.v
	$(INSTALL_DATA) point_slave_io.v $(DESTDIR)$(datadir)/point_slave_io.v

installdirs: ../mkinstalldirs
	$(srcdir)/../mkinstalldirs $(DESTDIR)$(datadir)
