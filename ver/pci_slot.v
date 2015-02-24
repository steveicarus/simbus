
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
 * NOTE: The PCIXCAP port is an OUTPUT. Simulations do not normally
 * use the PCIXCAP signal (it is implemented via discretes and traces)
 * but for some simulation purposes it may be useful to know what the
 * simulation thinks the bus is. It will be 1 if PCI-X is enabled, and
 * 0 otherwise.
 *
 * This port is controlled in the bus section of the server configuration
 * file by the setting of the PCIXCAP variable. Set it to "no" to force
 * this signal off, and "yes" to force it on. The entire bus shares
 * the same PCIXCAP value.
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
		output reg 	  PCI_CLK,
		output reg 	  RESET_n,
		// The bus implements IDSEL
		output wire 	  IDSEL,
		output reg 	  PCIXCAP,
		// Arbiter signals
		output reg 	  GNT_n,
		input wire 	  REQ_n,
		// Interrupt pins
		input wire 	  INTA_n,
		input wire 	  INTB_n,
		input wire 	  INTC_n,
		input wire 	  INTD_n,
		// Other PCI signals
		inout wire 	  FRAME_n,
		inout wire 	  REQ64_n,
		inout wire 	  IRDY_n,
		inout wire 	  TRDY_n,
		inout wire 	  STOP_n,
		inout wire 	  DEVSEL_n,
		inout wire 	  ACK64_n,
		inout wire [7:0]  C_BE,
		inout wire [63:0] AD,
		inout wire 	  PAR,
		inout wire 	  PAR64);

   // This is the name that I report to the bus server. The user
   // *must* override this, either by defparam or parameter override.
   parameter name = "N/A";

   // Normally, this port includes bus pullups that are required
   // for the PCI on the bus-side. Suppress those pullups by setting
   // this parameter to 0.
   parameter include_bus_pullups = 1;

   // These are values that the remote drives to the PCI port signals.
   // When the values are z, the remote is explicitly not driving.
   reg 	      idsel_drv = 1'bz; 
   reg 	      gnt_n_drv = 1'bz;
   reg 	      frame_n_drv = 1'bz;
   reg        req64_n_drv = 1'bz;
   reg 	      irdy_n_drv = 1'bz;
   reg 	      trdy_n_drv = 1'bz;
   reg 	      stop_n_drv = 1'bz;
   reg 	      devsel_n_drv = 1'bz;
   reg        ack64_n_drv  = 1'bz;
   reg [7:0]  c_be_drv = 8'hzz;
   reg [63:0] ad_drv = 64'hzzzzzzzz_zzzzzzzz;
   reg 	      par_drv = 1'bz;
   reg 	      par64_drv = 1'bz;

   // These are intermediate temporaries that we use to convert the
   // blocking assigns of the $simbus_until function into non-blocking
   // assignments for better synchronous behavior.
   reg 	      frame_n_tmp;
   reg        req64_n_tmp;
   reg 	      irdy_n_tmp;
   reg 	      trdy_n_tmp;
   reg 	      stop_n_tmp;
   reg 	      devsel_n_tmp;
   reg        ack64_n_tmp;
   reg        idsel_tmp;
   reg [7:0]  c_be_tmp;
   reg [63:0] ad_tmp;
   reg 	      par_tmp;
   reg 	      par64_tmp;

   assign FRAME_n = frame_n_tmp;
   assign REQ64_n = req64_n_tmp;
   assign IRDY_n  = irdy_n_tmp;
   assign TRDY_n  = trdy_n_tmp;
   assign STOP_n  = stop_n_tmp;
   assign DEVSEL_n= devsel_n_tmp;
   assign ACK64_n = ack64_n_tmp;
   assign IDSEL   = idsel_tmp;
   assign C_BE    = c_be_tmp;
   assign AD      = ad_tmp;
   assign PAR     = par_tmp;
   assign PAR64   = par64_tmp;

   if (include_bus_pullups) begin
      pullup frame_pull (FRAME_n);
      pullup req64_pull (REQ64_n);
      pullup irdt_pull  (IRDY_n);
      pullup trdy_pull  (TRDY_n);
      pullup stop_pull  (STOP_n);
      pullup devsel_pull(DEVSEL_n);
      pullup ack64_pull (ACK64_n);
      pullup idsel_pull (IDSEL);
      pullup c_be_pull[7:0] (C_BE);
      pullup ad_pull [63:0] (AD);
      pullup par_pull   (PAR);
      pullup par64_pull (PAR64);
   end

   time  deltatime;
   integer bus;

   reg 	   trig;

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
		       "REQ#",   REQ_n,     1'bz,
		       "INTA#",  INTA_n,    1'bz,
		       "INTB#",  INTB_n,    1'bz,
		       "INTC#",  INTC_n,    1'bz,
		       "INTD#",  INTD_n,    1'bz,
		       "FRAME#", FRAME_n,   frame_n_drv,
		       "REQ64#", REQ64_n,   req64_n_drv,
		       "IRDY#",  IRDY_n,    irdy_n_drv,
		       "TRDY#",  TRDY_n,    trdy_n_drv,
		       "STOP#",  STOP_n,    stop_n_drv,
		       "DEVSEL#",DEVSEL_n,  devsel_n_drv,
		       "ACK64#", ACK64_n,   ack64_n_drv,
		       "C/BE#",  C_BE,      c_be_drv,
		       "AD",     AD,        ad_drv,
		       "PAR",    PAR,       par_drv,
		       "PAR64",  PAR64,     par64_drv);

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
				   "PCI_CLK",PCI_CLK,
				   "RESET#", RESET_n,
				   "PCIXCAP",PCIXCAP,
				   "GNT#",   gnt_n_drv,
				   "IDSEL",  idsel_drv,
				   "FRAME#", frame_n_drv,
				   "REQ64#", req64_n_drv,
				   "IRDY#",  irdy_n_drv,
				   "TRDY#",  trdy_n_drv,
				   "STOP#",  stop_n_drv,
				   "DEVSEL#",devsel_n_drv,
				   "ACK64#", ack64_n_drv,
				   "C/BE#",  c_be_drv,
				   "AD",     ad_drv,
				   "PAR",    par_drv,
				   "PAR64",  par64_drv);

	 // This forces the actual assignments to the bus to be done
	 // by non-blocking assignments. This prevents races with the
	 // clock that is also delivered by the $simbus_until.
	 GNT_n        <= gnt_n_drv;
	 frame_n_tmp  <= frame_n_drv;
	 req64_n_tmp  <= req64_n_drv;
	 irdy_n_tmp   <= irdy_n_drv;
	 trdy_n_tmp   <= trdy_n_drv;
	 stop_n_tmp   <= stop_n_drv;
	 devsel_n_tmp <= devsel_n_drv;
	 ack64_n_tmp  <= ack64_n_drv;
	 c_be_tmp     <= c_be_drv;
	 idsel_tmp    <= idsel_drv;
	 ad_tmp       <= ad_drv;
	 par_tmp      <= par_drv;
	 par64_tmp    <= par64_drv;
      end

   end

endmodule // pci_port
