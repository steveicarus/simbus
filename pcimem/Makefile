
include ../Make.rules

all: pcimem.out

clean:
	rm -f pcimem.out *~

pcimem.out: pcimem.v ../ver/pci_slot.v ../vpi/simbus.sft
	$(IVERILOG) -o pcimem.out -y ../ver ../vpi/simbus.sft pcimem.v
