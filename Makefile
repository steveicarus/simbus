
include Make.rules

all: Make.rules
	cd server    ; $(MAKE) all
	cd libsimbus ; $(MAKE) all
	cd vpi       ; $(MAKE) all
	cd ver       ; $(MAKE) all
	cd pcimem    ; $(MAKE) all

clean:
	cd server    ; $(MAKE) clean
	cd libsimbus ; $(MAKE) clean
	cd vpi       ; $(MAKE) clean
	cd ver       ; $(MAKE) clean
	cd pcimem    ; $(MAKE) clean
	rm -f *~

distclean: clean
	rm -f Make.rules config.status

install:
	cd server    ; $(MAKE) install
	cd libsimbus ; $(MAKE) install
	cd vpi       ; $(MAKE) install
	cd ver       ; $(MAKE) install

uninstall:
	cd server    ; $(MAKE) uninstall
	cd libsimbus ; $(MAKE) uninstall
	cd vpi       ; $(MAKE) uninstall
	cd ver       ; $(MAKE) uninstall

Make.rules: Make.rules.in
	./config.status
