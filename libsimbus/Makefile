

include ../Make.rules

all: libsimbus.a

clean:
	rm -f libsimbus.a *.o *~

install: all installdirs \
	$(libdir)/libsimbus.a \
	$(includedir)/simbus_base.h \
	$(includedir)/simbus_pci.h \
	$(includedir)/simbus_pci_memdev.h \
	$(includedir)/simbus_p2p.h \
	$(includedir)/simbus_axi4.h \
	$(includedir)/simbus_axi4_common.h \
	$(includedir)/simbus_pcie_tlp.h

uninstall:
	rm -f $(DESTDIR)$(libdir)/libsimbus.a
	rm -f $(DESTDIR)$(includedir)/simbus_base.h
	rm -f $(DESTDIR)$(includedir)/simbus_pci.h
	rm -f $(DESTDIR)$(includedir)/simbus_pci_memdev.h
	rm -f $(DESTDIR)$(includedir)/simbus_p2p.h
	rm -f $(DESTDIR)$(includedir)/simbus_axi4.h
	rm -f $(DESTDIR)$(includedir)/simbus_axi4s.h
	rm -f $(DESTDIR)$(includedir)/simbus_axi4_common.h
	rm -f $(DESTDIR)$(includedir)/simbus_pcie_tlp.h

L = simbus.o \
	simbus_axi4.o \
	simbus_axi4_read.o \
	simbus_axi4_slave.o \
	simbus_axi4_write.o \
	simbus_pci.o \
	simbus_pci_config.o \
	simbus_pci_rw32.o \
	simbus_pci_rw64.o \
	simbus_pci_target.o \
	simbus_pci_memdev.o \
	simbus_p2p.o \
	simbus_pcie_tlp.o \
	simbus_pcie_tlp_cfg.o \
	simbus_pcie_tlp_read.o \
	simbus_pcie_tlp_reset.o \
	simbus_pcie_tlp_tlp.o \
	simbus_pcie_tlp_write.o \
	simbus_version.o

libsimbus.a: $L
	rm -f libsimbus.a
	ar cq libsimbus.a $L

simbus.o: simbus.c simbus_priv.h
simbus_axi4.o: simbus_axi4.c simbus_axi4.h simbus_axi4_common.h simbus_axi4_priv.h simbus_priv.h
simbus_axi4_read.o: simbus_axi4_read.c simbus_axi4.h simbus_axi4_common.h simbus_axi4_priv.h simbus_priv.h
simbus_axi4_slave.o: simbus_axi4_slave.c simbus_axi4s.h simbus_axi4_common.h simbus_axi4_priv.h simbus_priv.h
simbus_axi4_write.o: simbus_axi4_write.c simbus_axi4.h simbus_axi4_common.h simbus_axi4_priv.h simbus_priv.h
simbus_pci.o: simbus_pci.c simbus_pci.h simbus_pci_priv.h simbus_priv.h
simbus_pci_config.o: simbus_pci_config.c simbus_pci.h simbus_pci_priv.h simbus_priv.h
simbus_pci_rw32.o: simbus_pci_rw32.c simbus_pci.h simbus_pci_priv.h simbus_priv.h
simbus_pci_rw64.o: simbus_pci_rw64.c simbus_pci.h simbus_pci_priv.h simbus_priv.h
simbus_pci_target.o: simbus_pci_target.c simbus_pci.h simbus_pci_priv.h simbus_priv.h
simbus_p2p.o: simbus_p2p.c simbus_p2p.h simbus_p2p_priv.h simbus_priv.h
simbus_pcie_tlp.o: simbus_pcie_tlp.c simbus_pcie_tlp.h simbus_pcie_tlp_priv.h simbus_priv.h
simbus_pcie_tlp_cfg.o: simbus_pcie_tlp_cfg.c simbus_pcie_tlp.h simbus_pcie_tlp_priv.h simbus_priv.h
simbus_pcie_tlp_read.o: simbus_pcie_tlp_read.c simbus_pcie_tlp.h simbus_pcie_tlp_priv.h simbus_priv.h
simbus_pcie_tlp_reset.o: simbus_pcie_tlp_reset.c simbus_pcie_tlp.h simbus_pcie_tlp_priv.h simbus_priv.h
simbus_pcie_tlp_tlp.o: simbus_pcie_tlp_tlp.c simbus_pcie_tlp.h simbus_pcie_tlp_priv.h simbus_priv.h
simbus_pcie_tlp_write.o: simbus_pcie_tlp_write.c simbus_pcie_tlp.h simbus_pcie_tlp_priv.h simbus_priv.h

simbus_version.o: simbus_version.c simbus_base.h

version simbus_version.c: ./make_version.sh
	@echo '#include "simbus_base.h"' > simbus_version.c
	@echo 'const char*simbus_version(void)' >> simbus_version.c
	@echo '{ return "'`sh make_version.sh`'"; }' >> simbus_version.c

simbus_pci_memdev.o: simbus_pci_memdev.c simbus_pci_memdev.h simbus_pci.h

$(libdir)/libsimbus.a: libsimbus.a
	$(INSTALL_DATA) libsimbus.a $(DESTDIR)$(libdir)/libsimbus.a

$(includedir)/simbus_base.h: simbus_base.h
	$(INSTALL_DATA) simbus_base.h $(DESTDIR)$(includedir)/simbus_base.h

$(includedir)/simbus_axi4_common.h: simbus_axi4_common.h
	$(INSTALL_DATA) simbus_axi4_common.h $(DESTDIR)$(includedir)/simbus_axi4_common.h

$(includedir)/simbus_axi4.h: simbus_axi4.h
	$(INSTALL_DATA) simbus_axi4.h $(DESTDIR)$(includedir)/simbus_axi4.h

$(includedir)/simbus_pci.h: simbus_pci.h
	$(INSTALL_DATA) simbus_pci.h $(DESTDIR)$(includedir)/simbus_pci.h

$(includedir)/simbus_pci_memdev.h: simbus_pci_memdev.h
	$(INSTALL_DATA) simbus_pci_memdev.h $(DESTDIR)$(includedir)/simbus_pci_memdev.h

$(includedir)/simbus_p2p.h: simbus_p2p.h
	$(INSTALL_DATA) simbus_p2p.h $(DESTDIR)$(includedir)/simbus_p2p.h

$(includedir)/simbus_pcie_tlp.h: simbus_pcie_tlp.h
	$(INSTALL_DATA) simbus_pcie_tlp.h $(DESTDIR)$(includedir)/simbus_pcie_tlp.h


installdirs: ../mkinstalldirs
	$(srcdir)/../mkinstalldirs $(DESTDIR)$(libdir) $(DESTDIR)$(includedir)
