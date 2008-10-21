
all: simbus_server

O = main.o service.o

simbus_server: $O
	$(CXX) $(LDFLAGS) -o simbus_server $O

main.o: main.cc priv.h
service.o: service.cc priv.h
