
/*
 * Copyright 2008 Stephen Williams <steve@icarus.com>
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
 * This module defines the PCI DUT interface to the remote PCI bus
 * simulator. This of it as the slot that you plug your card into.
 * There should be exactly 1 if these for each (pci) bus connect that
 * your device supports. You don't need to know anything about the
 * simulation, you just need to connect your DUT to this slot and use
 * the PCI signals in their usual way.
 *
 * **
 *
 * parameter name = "string"
 *
 *    This parameter must be overridden with the logical name for this
 *    device. The server uses the name to bind IDSEL signals and route
 *    the device interrupts.
 */
`timescale 1ps/1ps

module pci_slot(// The bus supplies the clock and reset
		output reg PCI_CLK,
		output reg RESET_n,
		// The bus implements IDSEL
		output reg IDSEL,
		// Arbiter signals
		output reg GNT_n,
		input wire REQ_n,
		// Interrupt pins
		input wire INTA_n,
		input wire INTB_n,
		input wire INTC_n,
		input wire INTD_n,
		// Other PCI signals
		inout wire FRAME_n,
		inout wire IRDY_n,
		inout wire TRDY_n,
		inout wire STOP_n,
		inout wire DEVSEL_n,
		inout wire [7:0] C_BE,
		inout wire [63:0] AD,
		inout wire  PAR);

   // This is the name that I report to the bus server. The user
   // *must* override this, either by defparam or parameter override.
   parameter name = "N/A";

   // These are values that the remote drives to the PCI port signals.
   // When the values are z, the remote is explicitly not driving.
   reg 	 frame_n_drv = 1'bz;
   reg 	 irdy_n_drv = 1'bz;
   reg 	 trdy_n_drv = 1'bz;
   reg 	 stop_n_drv = 1'bz;
   reg 	 devsel_n_drv = 1'bz;
   reg   c_be_drv = 8'hzz;
   reg   ad_drv = 64'hzzzzzzzz_zzzzzzzz;
   reg 	 par_drv = 1'bz;

   assign FRAME_n = frame_n_drv;
   assign IRDY_n  = irdy_n_drv;
   assign TRDY_n  = trdy_n_drv;
   assign STOP_n  = stop_n_srv;
   assign DEVSEL_n= devsel_n_srv;
   assign C_BE    = c_be_n;
   assign AD      = ad_drv;
   assign PAR     = par_drv;

   time  deltatime;
   integer bus;
   initial begin
      // This connects to the bus and sends a message that logically
      // attaches this design to the system. The name is used to distinguish
      // this device during run-time. The returned bus integer is a cookie
      // that must be passed to the other $simbus functions.
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
	 
	 #(deltatime) /* Wait for the next remote event. */;

	 // Report the current state of the pins at the connector,
	 // along with the current time. For the first iteration, this
	 // gets us synchronized with the server with the time.
	 $simbus_ready(bus,
		       "REQ#",   REQ_n,
		       "INTA#",  INTA_n,
		       "INTB#",  INTB_n,
		       "INTC#",  INTC_n,
		       "INTD#",  INTD_n,
		       "FRAME#", FRAME_n,
		       "IRDY#",  IRDY_n,
		       "TRDY#",  TRDY_n,
		       "STOP#",  STOP_n,
		       "DEVSEL#",DEVSEL_n,
		       "C_BE",   C_BE,
		       "AD",     AD,
		       "PAR",    PAR);

	 // The server responds to this task call when it is ready
	 // for this device to advance some more. This task waits
	 // for the GO message from the server, which includes the
	 // values to drive to the various signals that I indicate.
	 // When the server responds, this task assigns those values
	 // to the arguments, and returns. The $simbus_until function
	 // automatically converts the time delay from the server to
	 // the units of the local scope.
	 deltatime = $simbus_until(bus,
				   "PCI_CLK",PCI_CLK,
				   "RESET#", RESET_n,
				   "GNT#",   GNT_n,
				   "IDSEL",  IDSEL,
				   "FRAME#", frame_n_drv,
				   "IRDY#",  irdy_n_drv,
				   "TRDY#",  trdy_n_drv,
				   "STOP#",  stop_n_drv,
				   "DEVSEL#",devsel_n_drv,
				   "C_BE",   c_be_drv,
				   "AD",     ad_drv,
				   "PAR",    par_drv);
      end

   end

endmodule // pci_port
