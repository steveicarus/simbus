
include ../Make.rules

all: simbus.vpi

clean:
	rm -f simbus.vpi *.o *~

S = simbus.c simbus_mem.c priv.h

simbus.vpi: $S
	$(IVERILOG_VPI) --name=simbus $S
