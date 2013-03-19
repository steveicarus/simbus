
include ../Make.rules

all: simbus.vpi

install: all installdirs $(iverilog_vpi_dir)/simbus.vpi $(iverilog_vpi_dir)/simbus.sft

uninstall:
	rm -f $(DESTDIR)$(iverilog_vpi_dir)/simbus.vpi
	rm -f $(DESTDIR)$(iverilog_vpi_dir)/simbus.sft

clean:
	rm -f simbus.vpi *.o *~

S = simbus.c simbus_mem.c priv.h

simbus.vpi: $S simbus_version.c
	$(IVERILOG_VPI) --name=simbus $S simbus_version.c 

$(iverilog_vpi_dir)/simbus.vpi: simbus.vpi
	$(INSTALL_DATA) simbus.vpi $(DESTDIR)$(iverilog_vpi_dir)/simbus.vpi

$(iverilog_vpi_dir)/simbus.sft: simbus.sft
	$(INSTALL_DATA) simbus.sft $(DESTDIR)$(iverilog_vpi_dir)/simbus.sft


installdirs: ../mkinstalldirs
	$(srcdir)/../mkinstalldirs $(DESTDIR)$(iverilog_vpi_dir)

simbus_version.c: $S Makefile
	@echo "Making simbus_version.c"
	@echo '  /* DO NOT EDIT */' > simbus_version.c
	@echo '# include  "priv.h"' >> simbus_version.c
	@echo 'const char simbus_version[] = "'`git describe --tags --dirty`'";' >> simbus_version.c
