
include ../Make.rules

all:

clean:
	rm -f *~

install: all installdirs $(datadir)/pci_slot.v \
	$(datadir)/point_master_io.v $(datadir)/point_slave_io.v \
	$(datadir)/cameralink_send.v $(datadir)/cameralink_recv.v

uninstall:
	rm -f $(DESTDIR)$(datadir)/pci_slot.v
	rm -f $(DESTDIR)$(datadir)/point_slave_io.v
	rm -f $(DESTDIR)$(datadir)/point_master_io.v
	rm -f $(DESTDIR)$(datadir)/cameralink_recv.v
	rm -f $(DESTDIR)$(datadir)/cameralink_send.v


$(datadir)/pci_slot.v: pci_slot.v
	$(INSTALL_DATA) pci_slot.v $(DESTDIR)$(datadir)/pci_slot.v

$(datadir)/point_master_io.v: point_master_io.v
	$(INSTALL_DATA) point_master_io.v $(DESTDIR)$(datadir)/point_master_io.v

$(datadir)/point_slave_io.v: point_slave_io.v
	$(INSTALL_DATA) point_slave_io.v $(DESTDIR)$(datadir)/point_slave_io.v

$(datadir)/cameralink_send.v: cameralink_send.v
	$(INSTALL_DATA) cameralink_send.v $(DESTDIR)$(datadir)/cameralink_send.v

$(datadir)/cameralink_recv.v: cameralink_recv.v
	$(INSTALL_DATA) cameralink_recv.v $(DESTDIR)$(datadir)/cameralink_recv.v

installdirs: ../mkinstalldirs
	$(srcdir)/../mkinstalldirs $(DESTDIR)$(datadir)
