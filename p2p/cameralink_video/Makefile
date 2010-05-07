

all: cameralink_camera cameralink_capture

CFLAGS = -O -g -I../../libsimbus
LIBS = -L../../libsimbus -lsimbus

O_CAM = camera.o camera_load.o

cameralink_camera: $(O_CAM) ../../libsimbus/libsimbus.a
	$(CC) -o cameralink_camera $(O_CAM) $(LIBS) -lpng


camera.o: camera.c camera_priv.h ../../libsimbus/simbus_p2p.h
camera_load.o: camera_load.c camera_priv.h

O_CAP = capture.o

cameralink_capture: $(O_CAP) ../../libsimbus/libsimbus.a
	$(CC) -o cameralink_capture $(O_CAP) $(LIBS)

capture.o: capture.c ../../libsimbus/simbus_p2p.h
