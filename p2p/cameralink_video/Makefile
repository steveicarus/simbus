
include ../../Make.rules

all: cameralink_camera cameralink_capture

clean:
	rm -f cameralink_camera cameralink_capture *.o *~

install: all installdirs \
	$(bindir)/cameralink_camera \
	$(bindir)/cameralink_capture

uninstall:
	rm -f $(DESTDIR)$(bindir)/cameralink_camera
	rm -f $(DESTDIR)$(bindir)/cameralink_capture

CFLAGS = -O -g -I../../libsimbus
LIBS = -L../../libsimbus -lsimbus -lm

O_CAM = camera.o camera_load.o

cameralink_camera: $(O_CAM) ../../libsimbus/libsimbus.a
	$(CC) -o cameralink_camera $(O_CAM) $(LIBS) -lpng


camera.o: camera.c camera_priv.h ../../libsimbus/simbus_p2p.h
camera_load.o: camera_load.c camera_priv.h

O_CAP = capture.o

cameralink_capture: $(O_CAP) ../../libsimbus/libsimbus.a
	$(CC) -o cameralink_capture $(O_CAP) $(LIBS)

capture.o: capture.c ../../libsimbus/simbus_p2p.h

$(bindir)/cameralink_camera: cameralink_camera
	$(INSTALL_PROGRAM) cameralink_camera $(DESTDIR)$(bindir)/cameralink_camera

$(bindir)/cameralink_capture: cameralink_capture
	$(INSTALL_PROGRAM) cameralink_capture $(DESTDIR)$(bindir)/cameralink_capture

installdirs: ../../mkinstalldirs
	../../mkinstalldirs $(DESTDIR)/$(bindir)
