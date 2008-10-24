
VERILOG = iverilog

pcimem.out: pcimem.v ../ver/pci_slot.v ../vpi/simbus.sft
	$(VERILOG) -o pcimem.out -y ../ver ../vpi/simbus.sft pcimem.v
