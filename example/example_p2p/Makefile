

all: master slave

CFLAGS = -O -g -I../../libsimbus
LIBS = -L../../libsimbus -lsimbus

IVERILOG = iverilog

O = master.o

master: $O ../../libsimbus/libsimbus.a
	$(CC) -o master $O $(LIBS)

slave: slave.v ../../ver/point_slave_io.v
	$(IVERILOG) -o slave.out -y ../../ver slave.v
