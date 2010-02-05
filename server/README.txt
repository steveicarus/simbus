
SIMBUS SERVER

The simbus server takes in a configuration file that describes the
bus configuration for the system, and simulates that bus. The server
can support multiple busses, each with a different port. These busses
are described in the configuration file.


CONFIGURATION FILES SYNTAX

There can be any number of busses in this server, and in this
configuration file. Just include a bus section for each bus you want
to define. Note that the port must be unique. You can also have
multiple configuration files. Specify each with a "-c <cfg path>"
option on the server command line.

* Comments

Comments start with the "#" character, and can start anywhere on a
line. They are treated as white space.

* Bus descriptions

All the busses in a system are fully described by their bus
descriptions. The description names the bus, assigns a bus protocol,
describes the bus service port and enumerates all the bus clients. The
syntax is:

  bus {
    # Give the bus a symbolic name. This is used for human readable
    # messages. The name is optional, a default will be chosen if needed.
    name = "<bus name>";

    # Select the bus protocol for the bus. This is required. The
    # protocol is given as a string, that the server then looks up in a
    # data-base of protocol implementations.
    protocol = "<type>";

    # Specify the TCP network port to use. The server listens on this
    # port, and the clients connects to this port to attach to the bus.
    port = <n>;

    # The server also supports named pipes for the server. This
    # specifies the path to the pipe to use. You can only specify port
    # or pipe for the bus, not both.
    #pipe = "bus_server";

    # List all the devices that are expected. The simulation does not
    # start until all the listed devices attach and identify themselves.
    #
    # The keyword device vs host selects what kind of connections is
    # made to the bus, since bus protocols are typically asymetrical. For
    # example, a "host" on a PCI bus outputs the RESET# signal, but
    # clients input the RESET# signal. The syntax here does not limit
    # any particular combination of hosts and devices, although the bus
    # protocol might.
    #
    # The <n> is used to assign non-shared signals to the device.
    #
    # The <name> is used to match an incoming client with this
    # device. The client knows its name and includes it in the "hello"
    # message it sends.
    device <n> "<name>";
    host <n> "<name>";
  }

* Process descriptions

A typical simulation may consist of a varienty of devices attached to
a collection of busses. This could lead to many processes to be
started by hand. Although that is normally fine, sometimes it is the
case that we just want the server to start processes for us. The
process description describes programs that the server should start
for us. This is an optional convenience. It doesn't have to be
complete, or even used at all.

  process {
     # The name is an optional symbolic name.
     name = "<program name>";

     env "<name>" = "<value>";

     # The exec gives the script to execute. This is passed to the
     # system shell (i.e. /bin/sh -c <script>) so shell commands
     # are allowed.
     exec = "<script to execute>";

     # The stdin should be specified. If not, it defaults to /dev/null
     stdin = "<path>";

     # The stdout should be specified. If not, it defaults to
     # /dev/null. If you specify "-", the stdout will be attached to
     # the stdout of the server.
     stdout = "<path>";

     # The stderr goes here. Specify "-" if you want the stderr to be
     # combined with the the stdout.
     stderr = "<path>";
  }

CLIENT PROTOCOL

The client connects to exactly 1 bus, and all of its interactions are
within the scope of that bus. The server knows by the connection which
bus the client is trying to join.

The client protocol is executed entirely in ASCII text. The client
sends a command to the server and blocks waiting for a response. Each
request starts with a command name and ends with a newline.

Times are always given in seconds using scientific notation and an
integer mantissa. For example: 3s = 3e0, 1ns = 1e-9, .05ms = 5e-5 and
so on.

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
is driving, or Z for bits that it is not driving. If the signal is a
vector, then write the bits MSB first. If a signal is not specified,
then the server assumes it is unchanged from any previous value.

The usual response is an UNTIL string as follows:

  UNTIL <time> <name>=<value>...

The <time> is the new simulation time when the bus value is expected
to be changed again, and the <name>=<value> tokens are assignments of
the resolved values as the bus appears. These reflect the drivings and
non-drivings of all the other devices on the bus.

If the bus is in the process of shutting down, then the clients will
receive the FINISH command instead of the UNTIL command. The client
shall close the socket and is detached from the bus.

* FINISH

Tell the bus to finish the simulation. This is normally used by the
test bench device to terminate the entire simulation. This command has
no response. However, it causes the server to send FINISH commands to
all the clients, including this client, in order to close down all the
clients gracefully.
