
CXXFLAGS = -O -g -I../../libsimbus

LIBS = -L../../libsimbus -lsimbus

O = host.o

host: $O ../../libsimbus/libsimbus.a
	$(CXX) -o host $O $(LIBS)


host.o: host.cc ../../libsimbus/simbus_pci.h
