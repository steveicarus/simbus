
include ../Make.rules

all: simbus_server

clean:
	rm -f simbus_server *.o *~
	rm -f lex.config.c
	rm -f config.tab.cpp config.tab.hpp

install: all installdirs $(bindir)/simbus_server

uninstall:
	rm -f $(DESTDIR)$(bindir)/simbus_server

O = main.o service.o client.o protocol.o process.o \
AXI4Protocol.o \
PciProtocol.o \
PointToPoint.o \
PCIeTLP.o \
mt19937int.o \
config.tab.o lex.config.o lxt2_write.o simbus_version.o

S = main.cc client.cc process.cc protocol.cc PciProtocol.cc PointToPoint.cc \
    PCIeTLP.cc PCIeTLP.h \
    mt19937int.c \
    config.ypp config.lex lxt2_write.c lxt2_write.h \
    priv.h protocol.h client.h simtime.h PciProtocol.h PointToPoint.h

simbus_server: $O
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o simbus_server $O -lz -lbz2

config.tab.cpp config.tab.hpp: config.ypp
	$(BISON) -d -p config config.ypp

lex.config.c: config.lex
	$(FLEX) -P config config.lex

main.o: main.cc priv.h
service.o: service.cc priv.h protocol.h mt_priv.h simtime.h AXI4Protocol.h PointToPoint.h PciProtocol.h PCIeTLP.h client.h lxt2_write.h
client.o: client.cc priv.h client.h
process.o: process.cc priv.h
protocol.o: protocol.cc priv.h protocol.h mt_priv.h simtime.h client.h lxt2_write.h
AXI4Protocol.o: AXI4Protocol.cc priv.h protocol.h mt_priv.h simtime.h AXI4Protocol.h
PciProtocol.o: PciProtocol.cc priv.h protocol.h mt_priv.h simtime.h PciProtocol.h
PointToPoint.o: PointToPoint.cc priv.h protocol.h mt_priv.h simtime.h PointToPoint.h
PCIeTLP.o: PCIeTLP.cc priv.h protocol.h mt_priv.h simtime.h PCIeTLP.h
mt19937int.o: mt19937int.c mt_priv.h
config.tab.o: config.tab.cpp lex.config.c priv.h
lex.config.o: lex.config.c config.tab.hpp
lxt2_write.o: lxt2_write.c lxt2_write.h
simbus_version.o: simbus_version.cc priv.h

$(bindir)/simbus_server: simbus_server
	$(INSTALL_PROGRAM) simbus_server $(DESTDIR)$(bindir)/simbus_server

installdirs: ../mkinstalldirs
	$(srcdir)/../mkinstalldirs $(DESTDIR)$(bindir)

simbus_version.cc: $S Makefile
	@echo "Making simbus_version.cc"
	@echo '  /* DO NOT EDIT */' > simbus_version.cc
	@echo '# include  "priv.h"' >> simbus_version.cc
	@echo 'const char simbus_version[] = "'`git describe --tags --dirty`'";' >> simbus_version.cc
