
SIMBUS=../..
LIBDIR=$(SIMBUS)/libsimbus

all: master slave

CXXFLAGS = -O -g -I$(LIBDIR)

LIBS = -L$(LIBDIR) -lsimbus

O = master.o

master: $O $(LIBDIR)/libsimbus.a
	$(CXX) -o master $O $(LIBS)

slave: slave.v $(SIMBUS)/ver/axi4_slave_slot.v
	iverilog -o slave -y$(SIMBUS)/ver slave.v

