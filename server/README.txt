
SIMBUS SERVER

The simbus server takes in a configuration file that describes the
bus configuration for the system, and simulates that bus. The server
can support multiple busses, each with a different port. These busses
are described in the configuration file.


CONFIGURATION

bus <N> {
  # Specify the TCP network port to use. The server listens on this
  # port, and the clients connects to this port to attach to the bus.
  tcp_port = <n>;

  # List all the devices that are expected. The simulation does not
  # start until all the listed devices attach and identify themselves.
  # The <n> is used to assign IDSEL, REQ/GNT and interrupts to the
  # device.
  device <n> "<name>";
}

CLIENT PROTOCOL

The client connects to exactly 1 bus, and all of its interactions are
within the scope of that bus. The server knows by the connection which
bus the client is trying to join.

The client protocol is executed entirely in ASCII text. The client
sends a command to the server and blocks waiting for a response. Each
request starts with a command name and ends with a newline.

Times are always given in seconds, with infinite precision.

* HELLO "<name>"

The client declares itself as a device by sending this command. The
server responds with "YOU-ARE <n>" if the server is expecting the
device, or "NAK". The client uses the <n> to select the specific
instance of non-shared signals.

* READY <time> <name>=<value>...

This client declares that it is ready for the next time step by
sending this command with the current time. The
<name>=<value>... tokens are the new values that the client is driving
onto the named bus signals. These are only the values that the client
is driving, or Z for bits that it is not driving. The response is an
UNTIL string as follows:

  UNTIL <time> <name>=<value>...

The <time> is the new simulation time when the bus value is expected
to be changed again, and the <name>=<value> tokens are assignments of
the resolved values as the bus appears. These reflect the drivings and
non-drivings of all the other devices on the bus.

* FINISH

Tell the bus to finish the simulation. This is normally used by the
test bench device to terminate the entire simulation.
