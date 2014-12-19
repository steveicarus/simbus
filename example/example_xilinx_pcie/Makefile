
SIMBUS=../..
LIBDIR=$(SIMBUS)/libsimbus

all: example_root example_endpoint

CXXFLAGS = -O -g -I$(LIBDIR)

LIBS = -L$(LIBDIR) -lsimbus

O = example_root.o

example_root: $O $(LIBDIR)/libsimbus.a
	$(CXX) -o example_root $O $(LIBS)

example_endpoint: example_endpoint.v $(SIMBUS)/ver/xilinx_pcie_slot.v
	iverilog -o example_endpoint -y$(SIMBUS)/ver example_endpoint.v

