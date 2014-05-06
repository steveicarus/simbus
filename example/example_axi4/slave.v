
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
 * This main module is a simple top-level that connects an axi4_slave_slot
 * to the sample register file that has an AXI4 interface. Notice that all
 * that is necessary is to wire the ports together. Easy! In a real design,
 * this "main" module would be part of the test bench and the design under
 * test in a separate file.
 *
 * This program can be programed by these parameters:
 * 
 *  * data_width (default: 32)
 *      Width of the words of the register file.
 * 
 *  * addr_width (default: 5)
 *      log2 of the number of words in the file. So for example with
 *      addr_width=5, the address is 5 bits wide and can address 32
 *      register_file words.
 */
module main;

   parameter data_width = 32;
   parameter addr_width = 5;
   parameter rid_width  = 4;
   parameter wid_width  = 4;

   localparam strb_width = data_width/8;

   // Global signals
   wire                  ACLK;
   wire                  ARESETn;
   // Write address channel
   wire                  AWVALID;
   wire                  AWREADY;
   wire [addr_width-1:0] AWADDR;
   wire [7:0] 		 AWLEN;
   wire [2:0] 		 AWSIZE;
   wire [1:0] 		 AWBURST;
   wire [1:0] 		 AWLOCK;
   wire [3:0] 		 AWCACHE;
   wire [2:0] 		 AWPROT;
   wire [3:0] 		 AWQOS;
   wire [wid_width-1:0]  AWID;
   // Write data channel
   wire 		 WVALID;
   wire 		 WREADY;
   wire [data_width-1:0] WDATA;
   wire [strb_width-1:0] WSTRB;
   // Write response channel
   wire 		 BVALID;
   wire 		 BREADY;
   wire [1:0]		 BRESP;
   wire [wid_width-1:0]  BID;
   // Read address channel
   wire 		 ARVALID;
   wire 		 ARREADY;
   wire [addr_width-1:0] ARADDR;
   wire [7:0] 		 ARLEN;
   wire [2:0] 		 ARSIZE;
   wire [1:0] 		 ARBURST;
   wire [1:0] 		 ARLOCK;
   wire [3:0] 		 ARCACHE;
   wire [2:0] 		 ARPROT;
   wire [3:0] 		 ARQOS;
   wire [rid_width-1:0]  ARID;
   // Read data channel
   wire 		 RVALID;
   wire 		 RREADY;
   wire [data_width-1:0] RDATA;
   wire [1:0] 		 RRESP;
   wire [rid_width-1:0]  RID;


   axi4_slave_slot #(.name("slave"), .data_width(data_width), .addr_width(addr_width)) slot
     (// Global signals
      .ACLK(ACLK),
      .ARESETn(ARESETn),
      // Write address channel
      .AWVALID(AWVALID),
      .AWREADY(AWREADY),
      .AWADDR(AWADDR),
      .AWLEN(AWLEN),
      .AWSIZE(AWSIZE),
      .AWBURST(AWBURST),
      .AWLOCK(AWLOCK),
      .AWCACHE(AWCACHE),
      .AWPROT(AWPROT),
      .AWQOS(AWQOS),
      .AWID(AWID),
      // Write data channel
      .WVALID(WVALID),
      .WREADY(WREADY),
      .WDATA(WDATA),
      .WSTRB(WSTRB),
      // Write response channel
      .BVALID(BVALID),
      .BREADY(BREADY),
      .BRESP(BRESP),
      .BID(BID),
      // Read address channel
      .ARVALID(ARVALID),
      .ARREADY(ARREADY),
      .ARADDR(ARADDR),
      .ARLEN(ARLEN),
      .ARSIZE(ARSIZE),
      .ARBURST(ARBURST),
      .ARLOCK(ARLOCK),
      .ARCACHE(ARCACHE),
      .ARPROT(ARPROT),
      .ARQOS(ARQOS),
      .ARID(ARID),
      // read data channel
      .RVALID(RVALID),
      .RREADY(RREADY),
      .RDATA(RDATA),
      .RRESP(RRESP),
      .RID(RID)
      /* */);

   register_file #(.data_width(data_width), .addr_width(addr_width)) dev
     (// Global signals
      .ACLK(ACLK),
      .ARESETn(ARESETn),
      // Write address channel
      .AWVALID(AWVALID),
      .AWREADY(AWREADY),
      .AWADDR(AWADDR),
      .AWPROT(AWPROT),
      // Write data channel
      .WVALID(WVALID),
      .WREADY(WREADY),
      .WDATA(WDATA),
      .WSTRB(WSTRB),
      // Write response channel
      .BVALID(BVALID),
      .BREADY(BREADY),
      .BRESP(BRESP),
      // Read address channel
      .ARVALID(ARVALID),
      .ARREADY(ARREADY),
      .ARADDR(ARADDR),
      .ARPROT(ARPROT),
      // read data channel
      .RVALID(RVALID),
      .RREADY(RREADY),
      .RDATA(RDATA),
      .RRESP(RRESP)
      /* */);

   initial $dumpvars(99, dev);

endmodule // main

module register_file
  #(parameter data_width = 32,
    parameter addr_width = 32,
    parameter strb_width = data_width/8
    /* */)
   (// Global signals
    input wire                  ACLK,
    input wire                  ARESETn,
    // Write address channel
    input wire                  AWVALID,
    output reg                  AWREADY,
    input wire [addr_width-1:0] AWADDR,
    input wire [2:0]            AWPROT,
    // Write data channel
    input wire                  WVALID,
    output reg                  WREADY,
    input wire [data_width-1:0] WDATA,
    input wire [strb_width-1:0] WSTRB,
    // Write response channel
    output reg                  BVALID,
    input wire                  BREADY,
    output reg [1:0]            BRESP,
    // Read address channel
    input wire                  ARVALID,
    output reg                  ARREADY,
    input wire [addr_width-1:0] ARADDR,
    input wire [2:0]            ARPROT,
    // Read data channel
    output reg                  RVALID,
    input wire                  RREADY,
    output reg [data_width-1:0] RDATA,
    output reg [1:0]            RRESP
    /* */);

   localparam                   bytes_per_word = data_width/8;
   localparam                   byte_count = 1<<addr_width;
   reg [7:0] 			mem [byte_count-1:0];

   initial begin
      $display("%m: register file holds %0d bytes of %0d bytes per word.", byte_count, bytes_per_word);
      $display("%m: addr_width=%0d", addr_width);
      $display("%m: data_width=%0d", data_width);
      $display("%m: strb_width=%0d", strb_width);
   end

   integer 			idx;

   function [addr_width-1:0] aligned_addr(input [addr_width-1:0] addr);
      aligned_addr = addr - (addr%bytes_per_word);
   endfunction // aligned_addr

   // Simulate the write port.
   reg [addr_width-1:0] 	write_addr;
   always @(posedge ACLK)
     if (ARESETn == 0) begin
	AWREADY <= 1;
	WREADY  <= 0;
	BVALID  <= 0;
	BRESP <= 0;

     end else begin
	if (AWREADY & AWVALID) begin
	   write_addr <= aligned_addr(AWADDR);
	   AWREADY <= 0;
	   WREADY  <= 1;
	end else if (AWREADY==0 & AWVALID==0) begin
	   AWREADY <= 1;
	end

	if (WREADY & WVALID) begin
	   for (idx = 0 ; idx < bytes_per_word ; idx = idx+1) begin
	     if (WSTRB[idx])
	       mem[write_addr+idx] <= WDATA[8*idx +: 8];
	   end
	   WREADY <= 0;
	   BVALID <= 1;
	end

	if (BVALID & BREADY) begin
	   BVALID <= 0;
	end
     end

   // Simulate the read port.
   reg [data_width-1:0] read_data;
   always @(posedge ACLK)
     if (ARESETn == 0) begin
	ARREADY <= 1;

     end else begin
	if (ARREADY & ARVALID) begin
	   for (idx = 0 ; idx < bytes_per_word ; idx = idx+1) begin
	      read_data[8*idx +: 8] = mem[aligned_addr(ARADDR)+idx];
	   end
	   ARREADY <= 0;
	   RDATA   <= read_data;
	   RRESP   <= 0;
	   RVALID  <= 1;

	end else if (RREADY & RVALID) begin
	   ARREADY <= 1;
	   RVALID  <= 0;
	end
     end

endmodule // register_file
