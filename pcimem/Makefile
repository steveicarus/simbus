
include ../Make.rules

all: pcimem.out

install: all installdirs $(datadir)/pcimem.out

uninstall:
	rm -f $(DESTDIR)$(datadir)/pcimem.out

clean:
	rm -f pcimem.out *~

pcimem.out: pcimem.v ../ver/pci_slot.v ../vpi/simbus.sft
	$(IVERILOG) -o pcimem.out -y ../ver ../vpi/simbus.sft pcimem.v

$(datadir)/pcimem.out: pcimem.out
	$(INSTALL_DATA) pcimem.out $(DESTDIR)$(datadir)/pcimem.out

installdirs: ../mkinstalldirs
	$(srcdir)/../mkinstalldirs $(DESTDIR)$(datadir)
