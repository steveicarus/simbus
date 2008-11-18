
include ../Make.rules

all: simbus_server

clean:
	rm -f simbus_server *.o *~
	rm -f lex.config.c
	rm -f config.tab.cpp config.tab.hpp

install: all

O = main.o service.o client.o protocol.o PciProtocol.o \
config.tab.o lex.config.o lxt2_write.o

simbus_server: $O
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o simbus_server $O -lz -lbz2

config.tab.cpp config.tab.hpp: config.ypp
	$(BISON) -d -p config config.ypp

lex.config.c: config.lex
	$(FLEX) -P config config.lex

main.o: main.cc priv.h
service.o: service.cc priv.h protocol.h PciProtocol.h client.h lxt2_write.h
client.o: client.cc priv.h client.h
protocol.o: protocol.cc priv.h protocol.h client.h lxt2_write.h
PciProtocol.o: PciProtocol.cc priv.h protocol.h PciProtocol.h
config.tab.o: config.tab.cpp lex.config.c priv.h
lex.config.o: lex.config.c config.tab.hpp
lxt2_write.o: lxt2_write.c lxt2_write.h
