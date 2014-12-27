/*
 * Copyright (c) 2014 Stephen Williams (steve@icarus.com)
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
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

`default_nettype none
`timescale 1ps/1ps

module main;

   wire user_clk_out;
   wire user_reset_out;

   wire tx_cfg_req;

   wire [63:0] s_axis_tx_tdata;
   wire [7:0]  s_axis_tx_tkeep;
   wire        s_axis_tx_tlast;
   wire        s_axis_tx_tready;
   wire        s_axis_tx_tvalid;
   wire [3:0]  s_axis_tx_tuser;

   wire [63:0] m_axis_rx_tdata;
   wire [7:0]  m_axis_rx_tkeep;
   wire        m_axis_rx_tlast;
   wire        m_axis_rx_tready;
   wire        m_axis_rx_tvalid;
   wire [21:0] m_axis_rx_tuser;

   xilinx_pcie_slot
     #(.dev_id("5555"),
       .ven_id("aaaa"),
       .subsys_id("1111"),
       .subsys_ven_id("bbbb")
       /* */)
   bus
     (/* */
      .user_clk_out(user_clk_out),
      .user_reset_out(user_reset_out),
      // Arbitration for the transmit channel. For now, just give
      // the cfg engine the bus  whenever it asks.
      .tx_cfg_req(tx_cfg_req),
      .tx_cfg_gnt(tx_cfg_req),

      .s_axis_tx_tdata(s_axis_tx_tdata),
      .s_axis_tx_tkeep(s_axis_tx_tkeep),
      .s_axis_tx_tlast(s_axis_tx_tlast),
      .s_axis_tx_tready(s_axis_tx_tready),

      .s_axis_tx_tuser(s_axis_tx_tuser),

      .m_axis_rx_tdata(m_axis_rx_tdata),
      .m_axis_rx_tkeep(m_axis_rx_tkeep),
      .m_axis_rx_tlast(m_axis_rx_tlast),
      .m_axis_rx_tready(m_axis_rx_tready),
      .m_axis_rx_tvalid(m_axis_rx_tvalid),
      .m_axis_rx_tuser(m_axis_rx_tuser)

      /* */);

   device dut
     (/* */
      .user_clk  (user_clk_out),
      .user_reset(user_reset_out),

      .s_axis_tx_tdata(s_axis_tx_tdata),
      .s_axis_tx_tkeep(s_axis_tx_tkeep),
      .s_axis_tx_tlast(s_axis_tx_tlast),
      .s_axis_tx_tready(s_axis_tx_tready),
      .s_axis_tx_tvalid(s_axis_tx_tvalid),
      .s_axis_tx_tuser(s_axis_tx_tuser),

      .m_axis_rx_tdata(m_axis_rx_tdata),
      .m_axis_rx_tkeep(m_axis_rx_tkeep),
      .m_axis_rx_tlast(m_axis_rx_tlast),
      .m_axis_rx_tready(m_axis_rx_tready),
      .m_axis_rx_tvalid(m_axis_rx_tvalid),
      .m_axis_rx_tuser(m_axis_rx_tuser)
      /* */);

   initial begin
      $dumpvars;
   end

endmodule // main

module device
  (/* */
   input wire 	     user_clk,
   input wire 	     user_reset,

   // Receive TLP stream
   input wire [63:0] m_axis_rx_tdata,
   input wire [7:0]  m_axis_rx_tkeep,
   input wire 	     m_axis_rx_tlast,
   output reg 	     m_axis_rx_tready,
   input wire 	     m_axis_rx_tvalid,
   input wire [21:0] m_axis_rx_tuser,

   output reg [63:0] s_axis_tx_tdata,
   output reg [7:0]  s_axis_tx_tkeep,
   output reg 	     s_axis_tx_tlast,
   input wire 	     s_axis_tx_tready,
   output reg 	     s_axis_tx_tvalid,
   output reg [3:0]  s_axis_tx_tuser
   /* */);

   reg 	      reset_flag = 0;
   always @(posedge user_clk) begin
      if (user_reset) begin
	 if (reset_flag == 0)
	   $display("Activate reset");
      end else begin
	 if (reset_flag == 1)
	   $display("Release reset");
      end
      reset_flag = user_reset;
   end

   // Buffer to receive TLPs.
   reg [31:0] tlp_buf [0:2047];
   reg [11:0] tlp_cnt;

   task collect_tlp_word(input [31:0] val);
      begin
	 tlp_buf[tlp_cnt] = val;
	 tlp_cnt = tlp_cnt+1;
      end
   endtask

   task complete_tlp;
      integer idx;
      begin
	 $display("%m: Received TLP at t=%0t", $time);
	 for (idx = 0 ; idx < tlp_cnt ; idx = idx+1) begin
	    $display("%m: %4d: %8h", idx, tlp_buf[idx]);
	 end

	 tlp_cnt = 0;
      end
   endtask

   always @(posedge user_clk)
     if (user_reset) begin
	tlp_cnt <= 0;
	m_axis_rx_tready <= 1;

     end else if (m_axis_rx_tvalid & m_axis_rx_tready) begin
	if (m_axis_rx_tkeep[3:0] == 4'b1111)
	  collect_tlp_word(m_axis_rx_tdata[31:0]);
	if (m_axis_rx_tkeep[7:4] == 4'b1111)
	  collect_tlp_word(m_axis_rx_tdata[63:32]);

	if (m_axis_rx_tlast)
	  complete_tlp;

     end else begin
     end

endmodule // device
