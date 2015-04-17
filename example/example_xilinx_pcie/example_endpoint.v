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

   wire        cfg_interrupt;
   wire        cfg_interrupt_rdy;
   wire        cfg_interrupt_assert;
   wire [7:0]  cfg_interrupt_di;

   xilinx_pcie_slot
     #(.dev_id("5555"),
       .ven_id("aaaa"),
       .subsys_id("1111"),
       .subsys_ven_id("bbbb"),
       .bar_0("FFFFF004"),
       .bar_1("FFFFFFFF")
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
      .s_axis_tx_tvalid(s_axis_tx_tvalid),

      .s_axis_tx_tuser(s_axis_tx_tuser),

      .m_axis_rx_tdata(m_axis_rx_tdata),
      .m_axis_rx_tkeep(m_axis_rx_tkeep),
      .m_axis_rx_tlast(m_axis_rx_tlast),
      .m_axis_rx_tready(m_axis_rx_tready),
      .m_axis_rx_tvalid(m_axis_rx_tvalid),
      .m_axis_rx_tuser(m_axis_rx_tuser),

      // Interrupt control flags
      .cfg_interrupt(cfg_interrupt),
      .cfg_interrupt_rdy(cfg_interrupt_rdy),
      .cfg_interrupt_assert(cfg_interrupt_assert),
      .cfg_interrupt_di(cfg_interrupt_di)
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
      .m_axis_rx_tuser(m_axis_rx_tuser),

      // Interrupt control flags
      .cfg_interrupt(cfg_interrupt),
      .cfg_interrupt_rdy(cfg_interrupt_rdy),
      .cfg_interrupt_assert(cfg_interrupt_assert),
      .cfg_interrupt_di(cfg_interrupt_di)
      /* */);

   initial begin
      $dumpvars;
   end

endmodule // main

/*
 * Address map:
 *     0x0000: Command register
 *         [0] -- Start writing data to arg1.
 *     0x0004: Arg1 register
 *
 *     0x0010: data...
 */
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
   output reg [3:0]  s_axis_tx_tuser,

   output reg 	     cfg_interrupt,
   input wire 	     cfg_interrupt_rdy,
   output reg 	     cfg_interrupt_assert,
   output reg [7:0]  cfg_interrupt_di
   /* */);

   // Keep a memory buffer that the remove can write/read.
   reg [31:0] 	     memory [0:8191];

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

   reg [31:0] otlp_buf [0:2047];
   reg [12:0] otlp_cnt, otlp_idx;

   task collect_tlp_word(input [31:0] val);
      begin
	 tlp_buf[tlp_cnt] = val;
	 tlp_cnt = tlp_cnt+1;
      end
   endtask

   function [31:0] byte_swap(input reg [31:0] val);
      begin
	 byte_swap = {val[7:0], val[15:8], val[23:16], val[31:24]};
      end
   endfunction

   task complete_tlp_write32;
      reg [31:0] addr, idx;
      reg [9:0]  ndata;
      begin
	 ndata = tlp_buf[0][9:0];
	 addr = tlp_buf[2][31:0];
	 $display("%m: Write %0d words to addr=%h", ndata, addr);
	 for (idx = 0 ; idx < ndata ; idx = idx+1)
	   memory[addr[11:2]+idx] = byte_swap(tlp_buf[3+idx]);
      end
   endtask

   task complete_tlp_write64;
      reg [63:0] addr, idx;
      reg [9:0]  ndata;
      begin
	 ndata = tlp_buf[0][9:0];
	 addr[63:32] = tlp_buf[2];
	 addr[31:0] = tlp_buf[3][31:0];
	 $display("%m: Write %0d words to addr=%h", ndata, addr);
	 for (idx = 0 ; idx < ndata ; idx = idx+1)
	   memory[addr[11:2]+idx] = byte_swap(tlp_buf[4+idx]);
      end
   endtask

   task complete_tlp_read32;
      reg [31:0] addr, idx;
      reg [9:0]  ndata;
      reg [7:0]  tag;
      begin
	 ndata = tlp_buf[0][9:0];
	 addr = tlp_buf[2][31:0];
	 tag = tlp_buf[1][15:8];

	 $display("%m: Read %0d words from addr=%h", ndata, addr);
	 // Build a completion w/ data.
	 otlp_buf[0] = {8'b010_01010, 14'h0, ndata};
	 otlp_buf[1] = 0;
	 otlp_buf[2] = {16'h00_00, tag, 8'h00};
	 for (idx = 0 ; idx < ndata ; idx = idx+1)
	   otlp_buf[3+idx] = byte_swap(memory[addr[11:2]+idx]);

	 otlp_cnt <= ndata + 3;
      end
   endtask //

   task complete_tlp_unknown;
      begin
	 $display("%m: Unknown TLP Fmt=%b, Type=%b", tlp_buf[0][31:29], tlp_buf[0][28:24]);
      end
   endtask

   task complete_tlp;
      integer idx;
      begin
	 $display("%m: Received TLP at t=%0t", $time);
	 for (idx = 0 ; idx < tlp_cnt ; idx = idx+1) begin
	    $display("%m: %4d: %8h", idx, tlp_buf[idx]);
	 end

	 case (tlp_buf[0][31:24])
	   'b010_00000: complete_tlp_write32;
	   'b011_00000: complete_tlp_write64;
	   'b000_00000: complete_tlp_read32;
	   default:     complete_tlp_unknown;
	 endcase // case (tlp_buf[0][31:24])

	 tlp_cnt = 0;
      end
   endtask

   // This state machine watches for arriving TLPs and collects
   // them into an input buffer. When the TLP is complete, it
   // calls the complete_tlp task to process it.
   always @(posedge user_clk)
     if (user_reset) begin
	tlp_cnt <= 0;
	m_axis_rx_tready <= 1;

     end else if (m_axis_rx_tvalid && m_axis_rx_tready) begin
	if (m_axis_rx_tkeep[3:0] == 4'b1111)
	  collect_tlp_word(m_axis_rx_tdata[31:0]);
	if (m_axis_rx_tkeep[7:4] == 4'b1111)
	  collect_tlp_word(m_axis_rx_tdata[63:32]);

	if (m_axis_rx_tlast)
	  complete_tlp;

     end else begin
     end

   // This state machine transmits TLPs. It senses that a TLP has been
   // placed in the otlp_buf buffer by noting the otlp_cnt status. When
   // it senses the TLP, it transmits the TLP and clears the otlp buffer.
   always @(posedge user_clk)
     if (user_reset) begin
	s_axis_tx_tvalid <= 0;
	s_axis_tx_tkeep  <= 0;
	s_axis_tx_tlast  <= 0;
	otlp_cnt <= 0;
	otlp_idx <= 0;

     end else if (otlp_idx<otlp_cnt && s_axis_tx_tvalid && s_axis_tx_tready) begin
	s_axis_tx_tvalid <= 1;
	if (otlp_cnt-otlp_idx >= 2) begin
	   s_axis_tx_tdata <= {otlp_buf[otlp_idx+1], otlp_buf[otlp_idx+0]};
	   s_axis_tx_tkeep <= 8'b1111_1111;
	   s_axis_tx_tlast <= (otlp_cnt-otlp_idx == 2)? 1 : 0;
	   otlp_idx <= otlp_idx + 2;
	end else begin
	   s_axis_tx_tdata <= {32'h00000000, otlp_buf[otlp_idx+0]};
	   s_axis_tx_tkeep <= 8'b0000_1111;
	   s_axis_tx_tlast <= 1;
	   otlp_idx <= otlp_idx + 1;
	end

     end else if (0<otlp_cnt && otlp_idx==0) begin
	s_axis_tx_tvalid <= 1;
	if (otlp_cnt >= 2) begin
	   s_axis_tx_tdata <= {otlp_buf[1], otlp_buf[0]};
	   s_axis_tx_tkeep <= 8'b1111_1111;
	   otlp_idx <= 2;
	end else begin
	   s_axis_tx_tdata <= {32'h00000000, otlp_buf[0]};
	   s_axis_tx_tkeep <= 8'b0000_1111;
	   otlp_idx <= 1;
	end
	s_axis_tx_tlast <= otlp_cnt<=2? 1 : 0;

     end else if (s_axis_tx_tvalid & s_axis_tx_tready) begin
	s_axis_tx_tvalid <= 0;
	s_axis_tx_tkeep <= 8'b0000_0000;
	s_axis_tx_tlast <= 0;
	otlp_idx <= 0;
	otlp_cnt <= 0;

     end else begin
     end

   // This state machine performs DMA writes when the write command is set.
   reg processing_dma_flag = 0;
   reg dma_done_interrupt = 0;
   always @(posedge user_clk)
     if (user_reset) begin
	memory[0][0] <= 0;
	processing_dma_flag <= 0;
	dma_done_interrupt <= 0;

     end else if (processing_dma_flag) begin
	if (otlp_cnt == 0) begin
	   dma_done_interrupt <= 1;
	end

     end else if (memory[0][0]) begin
	otlp_buf[0] = {8'b010_00000, 24'd4};
	otlp_buf[1] = {16'h00_00, 8'h00, 8'b1111_1111};
	otlp_buf[2] = memory[1];
	otlp_buf[3] = memory[4];
	otlp_buf[4] = memory[5];
	otlp_buf[5] = memory[6];
	otlp_buf[6] = memory[7];
	otlp_cnt <= 7;
	memory[0][0] <= 0;
	processing_dma_flag <= 1;
     end


   // This state machine transmits INTA interrupts to follow the
   // dma_done_interrupt flag.
   always @(posedge user_clk)
     if (user_reset) begin
	cfg_interrupt_di <= 0;
	cfg_interrupt <= 0;
	cfg_interrupt_assert <= 0;

     end else if (cfg_interrupt & cfg_interrupt_rdy) begin
	cfg_interrupt <= 0;

     end else if (cfg_interrupt) begin
	// Wait for cfg_interrupt_rdy
	
     end else if (cfg_interrupt_assert != dma_done_interrupt) begin
	cfg_interrupt_assert <= dma_done_interrupt;
	cfg_interrupt <= 1;

     end else begin
     end

endmodule // device
