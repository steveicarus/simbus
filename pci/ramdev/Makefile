

include ../../Make.rules

SIMBUS_DEP = ../../libsimbus/libsimbus.a

all: pci_ramdev

check: pci_ramdev check_ramdev

O = ramdev.o

pci_ramdev: $O $(SIMBUS_DEP)
	$(CXX) -L../../libsimbus -o pci_ramdev $O -lsimbus

%.o: %.cc
	$(CXX) $(CXXFLAGS) -I../../libsimbus -c $<

check_ramdev: check_ramdev.o $(SIMBUS_DEP)
	$(CXX) -L../../libsimbus -o check_ramdev check_ramdev.o -lsimbus

install: all installdirs $(bindir)/pci_ramdev

uninstall:
	rm -f $(DESTDIR)$(bindir)/pci_ramdev

$(bindir)/pci_ramdev: pci_ramdev
	$(INSTALL_PROGRAM) pci_ramdev $(DESTDIR)$(bindir)/pci_ramdev

installdirs: ../../mkinstalldirs
	$(srcdir)/../../mkinstalldirs $(DESTDIR)$(bindir)
