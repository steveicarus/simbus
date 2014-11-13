
SIMBUS=../..
LIBDIR=$(SIMBUS)/libsimbus

all: master slave

CXXFLAGS = -O -g -I$(LIBDIR)

LIBS = -L$(LIBDIR) -lsimbus

M = master.o

master: $M $(LIBDIR)/libsimbus.a
	$(CXX) -o master $M $(LIBS)

S = slave.o

slave: $S $(LIBDIR)/libsimbus.a
	$(CXX) -o slave $S $(LIBS)
