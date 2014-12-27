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
      // Arbitration for the transmit channel
      .tx_cfg_req(tx_cfg_req),
      .tx_cfg_gnt(tx_cfg_req)
      /* */);

   device dut
     (/* */
      .user_clk  (user_clk_out),
      .user_reset(user_reset_out)
      /* */);

   initial begin
      $dumpvars;
   end

endmodule // main

module device
  (/* */
   input wire user_clk,
   input wire user_reset
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

endmodule // device
