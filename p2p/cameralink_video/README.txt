
CameraLink Video Camera/Capture

These modules uses the point-to-point protocol to simulate a CameraLink
camera and/or a CameraLink capture device. Simulations that capture
data may use the camera as a video source that acts like a real
camera, and simulations that generate data may use the capture device
as a video sink.

* CameraLink Protocol

The video source (the camera) is configured as a "host" on a
point-to-point bus. The output data is the video (24bit RGB) and the
video control lines. There are also some input lines that the client
simulation can use to control the camera. (These are not part of the
usual CameraLink standard.) The server supplies the clock. This
protocol link does not include the UART that is also part of the
typical cabling.

The output bits are broken down as:

  DATA_O[ 7: 0]    -- Red[7:0]
  DATA_O[15: 8]    -- Green[7:0]
  DATA_O[23:16]    -- Blue[7:0]
  DATA_O[24]       -- VCE
  DATA_O[25]       -- LVV
  DATA_O[26]       -- FVV

The input bits are:

  DATA_I[0]        -- Camera Enable
  DATA_I[1]        -- Request Image
  DATA_I[7:2]      -- Reserved (drive 0)

The input bits are not part of the standard, but provide a way for
capture devices to control cameras. Since there is no transport that
triggers pages, the capture device acts to enable the camera and
initiate a scan.

* cameralink_camera

This is a simulation of a CameraLink camera. This device transmits RGB
data at the rate defined by the bus clock. It implements the
CameraEnable and RequestImage signals to manipulate the signals.

* cameralink_capture

This is a simulation of a CameraLink capture board. This device
receives RGB video at the rate defined by the bus clock, and writes
the images into files as they arrive.
