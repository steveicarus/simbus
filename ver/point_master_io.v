
/*
 * Copyright 2011 Stephen Williams <steve@icarus.com>
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */


/*
 * This module defines the point-to-point DUT interface to a remote
 * bus simulator. The remote server generates the clock and the output
 * data that are outputs from this module. The DUT drives the data_i
 * that is in turn sent to the remote. This module acts as a *master*
 * on the point-to-point bus. The slave is on the other end of the
 * bus and uses a slightly different module.
 *
 * **
 *
 * parameter name = "string"
 *
 *    This parameter must be overridden with the logical name for this
 *    device. The server uses the name to bind IDSEL signals and route
 *    the device interrupts.
 *
 * parameter WIDTH_O = <number>
 * parameter WIDTH_I = <number>
 *
 *    These parameters define the widths of the output (from master to
 *    slave) and input bus widths. The value must be >= 1.
 */
`timescale 1ps/1ps

module point_master_io
  #(// The name of this device in the bus
    parameter name = "N/A",
    parameter WIDTH_O = 1,
    parameter WIDTH_I = 1)

   (// The bus always supplies the clock
    output reg clock,
    input wire [WIDTH_O-1 : 0] data_o,
    output reg [WIDTH_I-1 : 0] data_i
    /* */);

   reg [WIDTH_I-1 : 0] 	       data_i_drv;

   time      deltatime;
   integer   bus;

   reg 	     trig;

   initial begin
      bus = $simbus_connect(name);
      if (bus < 0) begin
	 $display("ERROR: Unable to connect");
	 $finish;
      end

      deltatime = 1;
      forever begin
	 // The deltatime variable is the amount of time that this
	 // node is allowed to advance. The time is normally the time
	 // to the next clock edge or edge for any other asynchronous
	 // signal. On startup, this delay is non-zero but tiny so
	 // that the simulation can settle, and I have initial values
	 // to send to the server.
	 
	 #(deltatime) /* Wait for he next remote event */

	 // Report the current state of the pins at the connector,
	 // along with the current time. For the first iteration, this
	 // gets us synchronized with the server with the time.
	 $simbus_ready(bus, "DATA_O", data_o, {WIDTH_O{1'bz}});

	 // Check if the bus is ready for me to continue. The $simbus_poll
	 // function will set the trig to 0 or 1 depending on whether
	 // the $simbus_until function can continue without blocking.
	 // Wait if it will block. The poll will change the trig later
	 // once data arrives from the server.
	 $simbus_poll(bus, trig);
	 wait (trig) ;

	 // The server responds to this task call when it is ready
	 // for this device to advance some more. This task waits
	 // for the GO message from the server, which includes the
	 // values to drive to the various signals that I indicate.
	 // When the server responds, this task assigns those values
	 // to the arguments, and returns. The $simbus_until function
	 // automatically converts the time delay from the server to
	 // the units of the local scope.
	 deltatime = $simbus_until(bus,
				   "CLOCK",  clock,
				   "DATA_I", data_i_drv);

	 // This forces the actual assignments to the bus to be done
	 // by non-blocking assignments. This prevents races with the
	 // clock that is also delivered by the $simbus_until.
	 data_i <= data_i_drv;
      end
   end
endmodule
