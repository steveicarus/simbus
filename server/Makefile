
all: simbus_server

O = main.o service.o config.tab.o lex.config.o

simbus_server: $O
	$(CXX) $(LDFLAGS) -o simbus_server $O

config.tab.cpp config.tab.hpp: config.ypp
	bison -d -p config config.ypp

lex.config.c: config.lex
	flex -P config config.lex

main.o: main.cc priv.h
service.o: service.cc priv.h
config.tab.o: config.tab.cpp lex.config.c priv.h
lex.config.o: lex.config.c config.tab.hpp
