
`timescale 1ps/1ps

/*
 * Copyright 2014 Stephen Williams <steve@icarus.com>
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
 * This module represents the MASTER side that you hook to a SLAVE
 * device-under-test. The actual master implementation is the simbus
 * remote.
 * *
 * parameter name = "string"
 *
 *    This parameter must be overridded with the logical name for this
 *    device. The server uses this name to match up in the .bus config
 *    file.
 *
 * parameter data_width = 32
 *
 *    This parameter defines the data (read and write) width for the
 *    link. AXI4 in general support 8/16/32/64/128/256/512/1024 here,
 *    and AXI4-Lite supports 32/64.
 *
 * parmater addr_width = 32
 *
 *    This parameter defines the address width. The AXI4 standard doesn't
 *    place any constraints on this address.
 *
 * parameter irq_width = 1
 * 
 *    This parameter defines the number of interrupt lines from the device.
 *    AXI doesn't define interrupts, but this allows for it as a convenience
 *    for some real-world applications.
 */
module axi4_slave_slot
  #(parameter name = "N/A",     // bus name for simbus use
    parameter data_width = 32,  // data bus width
    parameter addr_width = 32,  // address width
    parameter wid_width = 4,    // write channel id width
    parameter rid_width = 4,    // read channel id width
    parameter irq_width = 1,    // Number of interrupt lines
    parameter strb_width = data_width/8
    /* */)
   (// Global signals
    output reg 			ACLK,
    output reg 			ARESETn,
    // Write address channel
    output reg 			AWVALID,
    input wire 			AWREADY,
    output reg [addr_width-1:0] AWADDR,
    output reg [7:0] 		AWLEN,
    output reg [2:0] 		AWSIZE,
    output reg [1:0] 		AWBURST,
    output reg [1:0] 		AWLOCK,
    output reg [3:0] 		AWCACHE,
    output reg [2:0] 		AWPROT,
    output reg [3:0] 		AWQOS,
    output reg [wid_width-1:0] 	AWID,
    // Write data channel
    output reg 			WVALID,
    input wire 			WREADY,
    output reg [data_width-1:0] WDATA,
    output reg [strb_width-1:0] WSTRB,
    // Write response channel
    input wire 			BVALID,
    output reg 			BREADY,
    input wire [1:0] 		BRESP,
    input wire [wid_width-1:0] 	BID,
    // Read address channel
    output reg 			ARVALID,
    input wire 			ARREADY,
    output reg [addr_width-1:0] ARADDR,
    output reg [7:0] 		ARLEN,
    output reg [2:0] 		ARSIZE,
    output reg [1:0] 		ARBURST,
    output reg [1:0] 		ARLOCK,
    output reg [3:0] 		ARCACHE,
    output reg [2:0] 		ARPROT,
    output reg [3:0] 		ARQOS,
    output reg [rid_width-1:0] 	ARID,
    // Read data channel
    input wire 			RVALID,
    output reg 			RREADY,
    input wire [data_width-1:0] RDATA,
    input wire [1:0] 		RRESP,
    input wire [rid_width-1:0] 	RID,
    // interrupts
    input wire [irq_width-1:0] 	IRQ
    /* */);

   time 			deltatime;
   integer 			bus;

   reg [32*8-1:0] 		data_width_tok;
   reg [32*8-1:0] 		addr_width_tok;
   reg [32*8-1:0] 		wid_width_tok;
   reg [32*8-1:0] 		rid_width_tok;
   reg [32*8-1:0] 		irq_width_tok;

   // Global signals
   reg 				ARESETn_drv;
   // Write address channel
   reg 				AWVALID_drv;
   reg [addr_width-1:0] 	AWADDR_drv;
   reg [7:0] 			AWLEN_drv;
   reg [2:0] 			AWSIZE_drv;
   reg [1:0] 			AWBURST_drv;
   reg [1:0] 			AWLOCK_drv;
   reg [3:0] 			AWCACHE_drv;
   reg [2:0] 			AWPROT_drv;
   reg [3:0] 			AWQOS_drv;
   reg [wid_width-1:0] 		AWID_drv;
   // Write data channel
   reg 				WVALID_drv;
   reg [data_width-1:0] 	WDATA_drv;
   reg [strb_width-1:0] 	WSTRB_drv;
   // Write response channel
   reg 				BREADY_drv;
   // Read address channel
   reg 				ARVALID_drv;
   reg [addr_width-1:0] 	ARADDR_drv;
   reg [7:0] 			ARLEN_drv;
   reg [2:0] 			ARSIZE_drv;
   reg [1:0] 			ARBURST_drv;
   reg [1:0] 			ARLOCK_drv;
   reg [3:0] 			ARCACHE_drv;
   reg [2:0] 			ARPROT_drv;
   reg [3:0] 			ARQOS_drv;
   reg [rid_width-1:0] 		ARID_drv;
   // Read data channel
   reg 				RREADY_drv;

   reg 				trig;
   initial begin
      // Connect to the server, and include my bus properties.
      $swrite(data_width_tok, "data_width=%0d", data_width);
      $swrite(addr_width_tok, "addr_width=%0d", addr_width);
      $swrite(wid_width_tok,  "wid_width=%0d",  wid_width);
      $swrite(rid_width_tok,  "rid_width=%0d",  rid_width);
      $swrite(irq_width_tok,  "irq_width=%0d",  irq_width);
      bus = $simbus_connect(name, data_width_tok, addr_width_tok, wid_width_tok, rid_width_tok, irq_width_tok);
      if (bus < 0) begin
	 $display("ERROR: Unable to connect");
	 $finish;
      end

      deltatime = 1;
      forever begin
	 #(deltatime) /* Wait for the next remote event. */ ;

	 $simbus_ready(bus,
		       // Global signals
		       // Write address channel
		       "AWREADY", AWREADY, 1'bz,
		       // Write data channel
		       "WREADY",  WREADY,  1'bz,
		       // Write response channel
		       "BVALID",  BVALID,  1'bz,
		       "BRESP",   BRESP,   2'bzz,
		       "BID",     BID,     {wid_width{1'bz}},
		       // Read address channel
		       "ARREADY", ARREADY, 1'bz,
		       // Read data channel
		       "RVALID",  RVALID,  1'bz,
		       "RDATA",   RDATA,   {data_width{1'bz}},
		       "RRESP",   RRESP,   2'bzz,
		       "RID",     RID,     {rid_width{1'bz}},
		       // Interrupt lines
		       "IRQ",     IRQ,     {irq_width{1'bz}}
		       /* */);

	 trig = 0;
	 $simbus_poll(bus, trig);
	 wait (trig) ;

	 deltatime = $simbus_until(bus,
				   // Global signals
				   "ACLK",    ACLK,
				   "ARESETn", ARESETn_drv,
				   // Write address channel
				   "AWVALID", AWVALID_drv,
				   "AWADDR",  AWADDR_drv,
				   "AWLEN",   AWLEN_drv,
				   "AWSIZE",  AWSIZE_drv,
				   "AWBURST", AWBURST_drv,
				   "AWLOCK",  AWLOCK_drv,
				   "AWCACHE", AWCACHE_drv,
				   "AWPROT",  AWPROT_drv,
				   "AWQOS",   AWQOS_drv,
				   "AWID",    AWID_drv,
				   // Write data channel
				   "WVALID",  WVALID_drv,
				   "WDATA",   WDATA_drv,
				   "WSTRB",   WSTRB_drv,
				   // Write response channel
				   "BREADY",  BREADY_drv,
				   // Read address channel
				   "ARVALID", ARVALID_drv,
				   "ARADDR",  ARADDR_drv,
				   "ARLEN",   ARLEN_drv,
				   "ARSIZE",  ARSIZE_drv,
				   "ARBURST", ARBURST_drv,
				   "ARLOCK",  ARLOCK_drv,
				   "ARCACHE", ARCACHE_drv,
				   "ARPROT",  ARPROT_drv,
				   "ARQOS",   ARQOS_drv,
				   "ARID",    ARID_drv,
				   // Read data channel
				   "RREADY",  RREADY_drv
				   );

	 // This forces the non-clock signals to be updated in the
	 // non-blocking assignment part of the time slot. This
	 // ensures proper relationship between the clock and these
	 // synchronous signals.
	 
	 // Global signals
	 ARESETn <= ARESETn_drv;
	 // Write address channel
	 AWVALID <= AWVALID_drv;
	 AWADDR  <= AWADDR_drv;
	 AWLEN   <= AWLEN_drv;
	 AWSIZE  <= AWSIZE_drv;
	 AWBURST <= AWBURST_drv;
	 AWLOCK  <= AWLOCK_drv;
	 AWCACHE <= AWCACHE_drv;
	 AWPROT  <= AWPROT_drv;
	 AWQOS   <= AWQOS_drv;
	 AWID    <= AWID_drv;
	 // Write data channel
	 WVALID  <= WVALID_drv;
	 WDATA   <= WDATA_drv;
	 WSTRB   <= WSTRB_drv;
	 // Write response channel
	 BREADY  <= BREADY_drv;
	 // Read address channel
	 ARVALID <= ARVALID_drv;
	 ARADDR  <= ARADDR_drv;
	 ARLEN   <= ARLEN_drv;
	 ARSIZE  <= ARSIZE_drv;
	 ARBURST <= ARBURST_drv;
	 ARLOCK  <= ARLOCK_drv;
	 ARCACHE <= ARCACHE_drv;
	 ARPROT  <= ARPROT_drv;
	 ARQOS   <= ARQOS_drv;
	 ARID    <= ARID_drv;
	 // Read data channel
	 RREADY  <= RREADY_drv;
      end // forever begin

   end
endmodule // axi4_slave_slot
