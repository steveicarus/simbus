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

/*
 * NOTES:
 * The transmit channel is mux'ed by the tx_cfg_gnt signal. the internal
 * core sometimes wants to take over the transmit channel, which it does
 * by asserting the tx_cfg_req signal. The user hands the bus over by
 * asserting the tx_cfg_gnt. The core holds the _req as long as it runs
 * a transaction. It will release the _req signal when it does not need
 * the channel, and the user can withdraw the grant.
 */
module xilinx_pcie_slot
  #(// PCIe is point-to-point, and this slot only simulates an endpoint,
    // so a good default for the device name is "endpoint". If the device
    // wants multiple endpoints, this will need to be overridden.
    parameter  name = "endpoint",

    parameter integer LINK_CAP_MAX_LINK_WIDTH = 6'h8,
    // Max payload supported: 0 - 128bytes, 1 - 256bytes, 2 - 512bytes, 3 - 1024bytes
    parameter integer DEV_CAP_MAX_PAYLOAD_SUPPORTED = 0,

    // Identifiers
    parameter ven_id        = "FFFF",
    parameter dev_id        = "FFFF",
    parameter rev_id        = "00",
    parameter subsys_ven_id = "FFFF",
    parameter subsys_id     = "FFFF",
    // BARs
    parameter bar_0    = "FFFFFFF4",
    parameter bar_1    = "FFFFFFFF",
    parameter bar_2    = "00000000",
    parameter bar_3    = "00000000",
    parameter bar_4    = "00000000",
    parameter bar_5    = "00000000",
    parameter xrom_bar = "00000000"
    /* */)
   (// The Xilinx X7 core interface are all synchronous with this
    // clock, so for hte purposes of simbus simulation we'll use this
    // as the simbus clock. We'll also let the remote control the
    // reset signal.
    output reg 				       user_clk_out,
    output reg 				       user_reset_out,
    // Link status from the remote.
    output reg 				       user_lnk_up,
    output reg 				       user_app_rdy,
 
    // The module being emulated has the clock and reset as inputs,
    // so here are place-holders.
    input wire 				       sys_clk,
    input wire 				       sys_rst_n,

    // Assuming these are only used for routing purposes, they are
    // only here in this simulation module for port compatibility.
    // The inputs are ignored, and the outputs are driven constant
    // so that the simulator can optimize them away.
    output reg [(LINK_CAP_MAX_LINK_WIDTH-1):0] pci_exp_txn,
    output reg [(LINK_CAP_MAX_LINK_WIDTH-1):0] pci_exp_txp,

    input wire [(LINK_CAP_MAX_LINK_WIDTH-1):0] pci_exp_rxn,
    input wire [(LINK_CAP_MAX_LINK_WIDTH-1):0] pci_exp_rxp,

    // Interface to the clock manager. These are stubbed locally.
    input wire 				       pipe_mmcm_rst_n,
    input wire 				       pipe_pclk_in,
    input wire 				       pipe_rxusrclk_in,
    input wire [1:0] 			       pipe_rxoutclk_in,
    input wire 				       pipe_userclk1_in,
    input wire 				       pipe_userclk2_in,
    input wire 				       pipe_mmcm_lock_in,
    input wire 				       pipe_dclk_in,
    input wire 				       pipe_oobclk_in,
    output wire 			       pipe_txoutclk_out,
    output wire [1:0] 			       pipe_rxoutclk_out,
    output wire [1:0] 			       pipe_pclk_sel_out,
    output wire 			       pipe_gen3_out,
    output reg [3:0] 			       pipe_cpll_lock,

    // Flow control status
    output reg [11:0] 			       fc_cpld,
    output reg [7:0] 			       fc_cplh,
    output reg [11:0] 			       fc_npd,
    output reg [7:0] 			       fc_nph,
    output reg [11:0] 			       fc_pd,
    output reg [7:0] 			       fc_ph,
    input wire [2:0] 			       fc_sel,

    // Transmit buffers available - This Verilog code passes TLPs
    // directly through, so this should be managed by the remote.
    output reg [5:0] 			       tx_buf_av,
    // Transmit buffer was dropped due to error
    output reg 				       tx_err_drop,

    output wire 			       tx_cfg_req,
    input wire 				       tx_cfg_gnt,

    // Transmit channel AXI4 stream (slave side)
    input wire [63:0] 			       s_axis_tx_tdata,
    input wire [7:0] 			       s_axis_tx_tkeep,
    input wire 				       s_axis_tx_tlast,
    output reg 				       s_axis_tx_tready,
    input wire 				       s_axis_tx_tvalid,
    input wire [3:0] 			       s_axis_tx_tuser,

    input wire 				       rx_np_ok,
    input wire 				       rx_np_req,

    // Receive channel AXI4 Stream (master side)
    // This stream carries TLPs from the remote (the root) to
    // this slot.
    output wire [63:0] 			       m_axis_rx_tdata,
    output wire [7:0] 			       m_axis_rx_tkeep,
    output wire 			       m_axis_rx_tlast,
    input wire 				       m_axis_rx_tready,
    output wire 			       m_axis_rx_tvalid,
    output wire [21:0] 			       m_axis_rx_tuser,

    // Configuration interface
    output reg [31:0] 			       cfg_mgmt_do,
    input wire [31:0] 			       cfg_mgmt_di,
    input wire [3:0] 			       cfg_mgmt_byte_en,
    input wire [9:0] 			       cfg_mgmt_dwaddr,
    input wire 				       cfg_mgmt_wr_en,
    input wire 				       cfg_mgmt_rd_en,
    input wire 				       cfg_mgmt_wr_readonly,
    output reg 				       cfg_mgmt_rd_wr_done,
    input wire 				       cfg_mgmt_wr_rw1c_as_rw,

    output wire [15:0] 			       cfg_status,
    output wire [15:0] 			       cfg_command,
    output wire [7:0] 			       cfg_bus_number,
    output wire [4:0] 			       cfg_device_number,
    output wire [2:0] 			       cfg_function_number,
    output wire [15:0] 			       cfg_dstatus,
    output wire [15:0] 			       cfg_dcommand,
    output wire [15:0] 			       cfg_lstatus,
    output wire [15:0] 			       cfg_lcommand,
    output reg [15:0] 			       cfg_dcommand2,
    output reg [2:0] 			       cfg_pcie_link_state,
    output reg 				       cfg_pmcsr_pme_en,
    output wire [1:0] 			       cfg_pmcsr_powerstate,
    output reg 				       cfg_pmcsr_pme_status,
    output reg 				       cfg_received_func_lvl_rst,

    input wire 				       cfg_err_ecrc,
    input wire 				       cfg_err_ur,
    input wire 				       cfg_err_cpl_timeout,
    input wire 				       cfg_err_cpl_unexpect,
    input wire 				       cfg_err_cpl_abort,
    input wire 				       cfg_err_posted,
    input wire 				       cfg_err_cor,
    input wire 				       cfg_err_atomic_egress_blocked,
    input wire 				       cfg_err_internal_cor,
    input wire 				       cfg_err_malformed,
    input wire 				       cfg_err_mc_blocked,
    input wire 				       cfg_err_poisoned,
    input wire 				       cfg_err_norecovery,
    input wire [47:0] 			       cfg_err_tlp_cpl_header,
    output wire 			       cfg_err_cpl_rdy,
    input wire 				       cfg_err_locked,
    input wire 				       cfg_err_acs,
    input wire 				       cfg_err_internal_uncor,

    input wire 				       cfg_trn_pending,

    input wire 				       cfg_pm_halt_aspm_l0s,
    input wire 				       cfg_pm_halt_aspm_l1,
    input wire 				       cfg_pm_force_state_en,
    input wire [1:0] 			       cfg_pm_force_state,
    input wire 				       cfg_pm_wake,
    input wire 				       cfg_pm_send_pme_to,

    input wire [63:0] 			       cfg_dsn,
    input wire 				       cfg_interrupt,
    output wire 			       cfg_interrupt_rdy,
    input wire 				       cfg_interrupt_assert,
    input wire [7:0] 			       cfg_interrupt_di,
    output reg [7:0] 			       cfg_interrupt_do,
    output reg [2:0] 			       cfg_interrupt_mmenable,
    output wire 			       cfg_interrupt_msienable,
    output wire 			       cfg_interrupt_msixenable,
    output reg 				       cfg_interrupt_msixfm,
    input wire 				       cfg_interrupt_stat,

    input wire [4:0] 			       cfg_pciecap_interrupt_msgnum,

    output reg 				       cfg_to_turnoff,
    input wire 				       cfg_turnoff_ok,

    input wire [7:0] 			       cfg_ds_bus_number,
    input wire [4:0] 			       cfg_ds_device_number,
    input wire [2:0] 			       cfg_ds_function_number,

    output reg 				       cfg_msg_received,
    output reg [15:0] 			       cfg_msg_data,

    output reg 				       cfg_bridge_serr_en,
    output reg 				       cfg_slot_control_electromech_il_ctl_pulse,
    output reg 				       cfg_root_control_syserr_corr_err_en,
    output reg 				       cfg_root_control_syserr_non_fatal_err_en,
    output reg 				       cfg_root_control_syserr_fatal_err_en,
    output reg 				       cfg_root_control_pme_int_en,
    output reg 				       cfg_aer_rooterr_corr_err_reporting_en,
    output reg 				       cfg_aer_rooterr_non_fatal_err_reporting_en,
    output reg 				       cfg_aer_rooterr_fatal_err_reporting_en,
    output reg 				       cfg_aer_rooterr_corr_err_received,
    output reg 				       cfg_aer_rooterr_non_fatal_err_received,
    output reg 				       cfg_aer_rooterr_fatal_err_received,
    output reg 				       cfg_msg_received_err_cor,
    output reg 				       cfg_msg_received_err_non_fatal,
    output reg 				       cfg_msg_received_err_fatal,
    output reg 				       cfg_msg_received_pm_as_nak,
    output reg 				       cfg_msg_received_pm_pme,
    output reg 				       cfg_msg_received_pme_to_ack,
    output reg 				       cfg_msg_received_assert_int_a,
    output reg 				       cfg_msg_received_assert_int_b,
    output reg 				       cfg_msg_received_assert_int_c,
    output reg 				       cfg_msg_received_assert_int_d,
    output reg 				       cfg_msg_received_deassert_int_a,
    output reg 				       cfg_msg_received_deassert_int_b,
    output reg 				       cfg_msg_received_deassert_int_c,
    output reg 				       cfg_msg_received_deassert_int_d,
    output reg 				       cfg_msg_received_setslotpowerlimit,
    input wire [1:0] 			       pl_directed_link_change,
    input wire [1:0] 			       pl_directed_link_width,
    input wire 				       pl_directed_link_auton,
    input wire 				       pl_directed_link_speed,
    input wire 				       pl_upstream_prefer_deemph,
    output wire 			       pl_sel_lnk_rate,
    output wire [1:0] 			       pl_sel_lnk_width,
    output wire [5:0] 			       pl_ltssm_state,
    output wire [1:0] 			       pl_lane_reversal_mode,
    output reg 				       pl_phy_lnk_up,
    output reg [2:0] 			       pl_tx_pm_state,
    output reg [1:0] 			       pl_rx_pm_state,
    output reg 				       pl_link_upcfg_cap,
    output reg 				       pl_link_gen2_cap,
    output reg 				       pl_link_partner_gen2_supported,
    output wire [2:0] 			       pl_initial_link_width,
    output reg 				       pl_directed_change_done,
    output reg 				       pl_received_hot_rst,
    input wire 				       pl_transmit_hot_rst,
    input wire 				       pl_downstream_deemph_source,

    input wire [127:0] 			       cfg_err_aer_headerlog,
    input wire [4:0] 			       cfg_aer_interrupt_msgnum,
    output reg 				       cfg_err_aer_headerlog_set,
    output reg 				       cfg_aer_ecrc_check_en,
    output reg 				       cfg_aer_ecrc_gen_en,
    output reg [6:0] 			       cfg_vc_tcvc_map,

    input wire 				       pcie_drp_clk,
    input wire 				       pcie_drp_en,
    input wire 				       pcie_drp_we,
    output reg 				       pcie_drp_rdy,
    input wire [8:0] 			       pcie_drp_addr,
    input wire [15:0] 			       pcie_drp_di,
    output reg [15:0] 			       pcie_drp_do

    /* */);


   // The core doesn't actually support this signal. Stub it out.
   assign cfg_status = 16'h00;
   assign cfg_pmcsr_powerstate = 2'b00;
   assign pipe_gen3_out = 1'd1;
   assign pipe_pclk_sel_out = 4'd0;
   assign pipe_txoutclk_out = {sys_clk, sys_clk};
   assign pipe_txoutclk_out = sys_clk;
   assign pl_sel_lnk_rate = 1'b0;
   assign pl_sel_lnk_width = 2'b10;
   assign pl_ltssm_state = 6'b000000;
   assign pl_lane_reversal_mode = 2'b00;
   assign pl_initial_link_width = 3'b010;

   // The *_drv signals are copies of the port signals that are driven
   // by the $simbus_until system function. After the UNTIL completes,
   // we assign them to the proper pins by a non-blocking assignment so
   // that the relationship with the clock is proper.

   // The *_int signals are copies of the port inputs that are driven
   // by internal state machines. We process some of the signals
   // internally, so we need an internal version of the channel that
   // doesn't interfere with the user signals.
   
   reg 	      user_reset_drv = 1'bz;
   reg 	      user_lnk_up_drv = 1'b0;
   reg [5:0]  tx_buf_av_drv = 6'hzz;

   // Sometimes the config engine needs to take control over the axis_tx
   // port. This is set true when that is allowed.
   reg 	      tx_cfg_gnt_int = 1'b0;

   reg [63:0] m_axis_rx_tdata_drv = 64'bz;
   reg [7:0]  m_axis_rx_tkeep_drv = 8'bz;
   wire       m_axis_rx_tready_int = 1'bz;
   reg        m_axis_rx_tlast_drv = 1'bz;
   reg        m_axis_rx_tvalid_drv = 1'bz;

   reg [63:0] m_axis_rx_tdata_int;
   reg [7:0]  m_axis_rx_tkeep_int;
   reg        m_axis_rx_tlast_int;
   reg        m_axis_rx_tvalid_int;

   wire [63:0] s_axis_tx_tdata_int = 64'bz;
   wire [7:0]  s_axis_tx_tkeep_int =  8'bz;
   reg 	       s_axis_tx_tready_drv = 1'bz;
   reg 	       s_axis_tx_tready_int = 1'bz;
   wire        s_axis_tx_tlast_int =  1'bz;
   wire        s_axis_tx_tvalid_int = 1'bz;
   wire [3:0]  s_axis_tx_tuser_int  = 4'bz;

   time      deltatime;
   integer   bus;
   reg 	     trig;

   initial begin
      $display("%m: xilinx_pcie_slot.v version: @XXXX@");

      // This connects to the bus and sends a message that logically
      // attaches the design to the system.
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
		       /* Receive Interace (to remote) */
		       "m_axis_rx_tready", m_axis_rx_tready_int, 1'bz,
		       /* Transmit Interface (to remote) */
		       "s_axis_tx_tdata",  tx_cfg_gnt_int? s_axis_tx_tdata_int  : s_axis_tx_tdata, 64'bz,
		       "s_axis_tx_tkeep",  tx_cfg_gnt_int? s_axis_tx_tkeep_int  : s_axis_tx_tkeep,  8'bz,
		       "s_axis_tx_tlast",  tx_cfg_gnt_int? s_axis_tx_tlast_int  : s_axis_tx_tlast,  1'bz,
		       "s_axis_tx_tvalid", tx_cfg_gnt_int? s_axis_tx_tvalid_int : s_axis_tx_tvalid, 1'bz,
		       "s_axis_tx_tuser",  tx_cfg_gnt_int? s_axis_tx_tuser_int  : s_axis_tx_tuser,  4'bz
		       /* */);

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
				   /* Common Interface (from remote) */
				   "user_clk",    user_clk_out,
				   "user_reset",  user_reset_drv,
				   "user_lnk_up", user_lnk_up_drv,
				   /* Receive channel (from remote) */
				   "m_axis_rx_tdata", m_axis_rx_tdata_drv,
				   "m_axis_rx_tkeep", m_axis_rx_tkeep_drv,
				   "m_axis_rx_tlast", m_axis_rx_tlast_drv,
				   "m_axis_rx_tvalid",m_axis_rx_tvalid_drv,
				   /* Transmite channel (from remote) */
				   "s_axis_tx_tready",s_axis_tx_tready_drv,
				   /* TX buffer management */
				   "tx_buf_av",       tx_buf_av_drv
				   /* */);

	 user_reset_out <= user_reset_drv;
	 user_lnk_up <= user_lnk_up_drv;
	 user_app_rdy <= 1'b1;

	 m_axis_rx_tdata_int  <= m_axis_rx_tdata_drv;
	 m_axis_rx_tkeep_int  <= m_axis_rx_tkeep_drv;
	 m_axis_rx_tlast_int  <= m_axis_rx_tlast_drv;
	 m_axis_rx_tvalid_int <= m_axis_rx_tvalid_drv;
	 tx_buf_av            <= tx_buf_av_drv;

	 if (tx_cfg_gnt_int) begin
	    s_axis_tx_tready_int <= s_axis_tx_tready_drv;
	    s_axis_tx_tready     <= 1'b0;
	 end else begin
	    s_axis_tx_tready_int <= 1'b0;
	    s_axis_tx_tready     <= s_axis_tx_tready_drv;
	 end
      end // forever begin
   end // initial begin

   // This state machine manages who gets control over the s_axis_tx port.
   // If the user starts a transaction, then it gets the port until it
   // sends the last beat. If the config space requests, and it granted,
   // the bus, then it gets it. Whoever has the port, it releases the
   // port when the last beat is transmitted.
   reg tx_user_busy;
   always @(posedge user_clk_out)
     if (user_reset_out) begin
	tx_cfg_gnt_int <= 0;
	tx_user_busy <= 0;

     end else if (tx_user_busy & s_axis_tx_tready & s_axis_tx_tvalid & s_axis_tx_tlast) begin
	// User gives up the port after the last beat
	tx_user_busy <= 0;

     end else if (tx_cfg_gnt_int & s_axis_tx_tready_int & s_axis_tx_tvalid_int & s_axis_tx_tlast_int) begin
	// CFg engine gives up the port after the last beat.
	tx_cfg_gnt_int <= 0;

     end else if (s_axis_tx_tvalid & ~tx_cfg_req & ~tx_user_busy) begin
	// User gets port when it starts a TLP
	tx_user_busy <= 1;

     end else if (s_axis_tx_tvalid & s_axis_tx_tready & ~tx_cfg_gnt_int) begin
	// User sneaks in another TLP before we can grant, so mark
	// that the user is busy sending a TLP.
	tx_user_busy <= 1;

     end else if (tx_cfg_req & tx_cfg_gnt & ~tx_user_busy) begin
	// Cfg engine gets port if requested, and granted.
	tx_cfg_gnt_int <= 1;

     end

   xilinx_pcie_cfg_space
     #(.ven_id(ven_id),
       .dev_id(dev_id),
       .rev_id(rev_id),
       .subsys_id(subsys_id),
       .subsys_ven_id(subsys_ven_id),
       .bar_0(bar_0),
       .bar_1(bar_1),
       .bar_2(bar_2),
       .bar_3(bar_3),
       .bar_4(bar_4),
       .bar_5(bar_5),
       .xrom_bar(xrom_bar)
       /* */) cfg_space
       (.user_clk(user_clk_out),
	.user_reset(user_reset_out),
	// Transmit channel arbitration
	.tx_cfg_req(tx_cfg_req),
	.tx_cfg_gnt(tx_cfg_gnt_int),
	// Receive channel AXI4 Stream
	.m_axis_rx_tdata (m_axis_rx_tdata_int),
	.m_axis_rx_tkeep (m_axis_rx_tkeep_int),
	.m_axis_rx_tlast (m_axis_rx_tlast_int),
	.m_axis_rx_tready(m_axis_rx_tready_int),
	.m_axis_rx_tvalid(m_axis_rx_tvalid_int),
	// Receive channel AXI4 Stream (filtered for user)
	.o_axis_rx_tdata (m_axis_rx_tdata),
	.o_axis_rx_tkeep (m_axis_rx_tkeep),
	.o_axis_rx_tlast (m_axis_rx_tlast),
	.o_axis_rx_tready(m_axis_rx_tready),
	.o_axis_rx_tvalid(m_axis_rx_tvalid),
	.o_axis_rx_tuser (m_axis_rx_tuser),
	// Transmit channel AXI4 Stream
	.s_axis_tx_tdata (s_axis_tx_tdata_int),
	.s_axis_tx_tkeep (s_axis_tx_tkeep_int),
	.s_axis_tx_tlast (s_axis_tx_tlast_int),
	.s_axis_tx_tready(s_axis_tx_tready_int),
	.s_axis_tx_tvalid(s_axis_tx_tvalid_int),
	.s_axis_tx_tuser (s_axis_tx_tuser_int),
	// dev/fun numbers collected from Cfg messages
	.cfg_command(cfg_command),
	.cfg_dcommand(cfg_dcommand),
	.cfg_dstatus(cfg_dstatus),
	.cfg_lcommand(cfg_lcommand),
	.cfg_lstatus(cfg_lstatus),
	.cfg_bus_number(cfg_bus_number),
	.cfg_dev_number(cfg_device_number),
	.cfg_fun_number(cfg_function_number),
	// Interrupt management
	.cfg_interrupt(cfg_interrupt),
	.cfg_interrupt_rdy(cfg_interrupt_rdy),
	.cfg_interrupt_assert(cfg_interrupt_assert),
	.cfg_interrupt_di(cfg_interrupt_di),
	.cfg_interrupt_msienable(cfg_interrupt_msienable),
	.cfg_interrupt_msixenable(cfg_interrupt_msixenable),
	// Error completions bits
	.cfg_err_cpl_rdy(cfg_err_cpl_rdy),
	// Various cfg control bits
	.cfg_trn_pending(cfg_trn_pending)
	/* */);

endmodule // xilinx_pcie_slot

/*
 * The PCIe core handles configuration space details internally, so this
 * module picks off config TLPs and handles them.
 */
module xilinx_pcie_cfg_space
  #(parameter ven_id = "FFFF",
    parameter dev_id = "FFFF",
    parameter rev_id = "00",
    parameter subsys_ven_id = "FFFF",
    parameter subsys_id     = "FFFF",
    // BARs
    parameter bar_0    = "FFFFFFFC",
    parameter bar_1    = "FFFFFFFF",
    parameter bar_2    = "00000000",
    parameter bar_3    = "00000000",
    parameter bar_4    = "00000000",
    parameter bar_5    = "00000000",
    parameter xrom_bar = "00000000"
    /* */)
   (input wire user_clk,
    input wire 	       user_reset,

    output reg 	       tx_cfg_req,
    input wire 	       tx_cfg_gnt,

    // Receive channel AXI4 Stream
    input wire [63:0]  m_axis_rx_tdata,
    input wire [7:0]   m_axis_rx_tkeep,
    input wire 	       m_axis_rx_tlast,
    output wire        m_axis_rx_tready,
    input wire 	       m_axis_rx_tvalid,

    // Receive channel AXI4 Stream (passed to user)
    output wire [63:0] o_axis_rx_tdata,
    output wire [7:0]  o_axis_rx_tkeep,
    output wire        o_axis_rx_tlast,
    input wire 	       o_axis_rx_tready,
    output wire        o_axis_rx_tvalid,
    output wire [21:0] o_axis_rx_tuser,

    // Transmit channel AXI4 stream
    output reg [63:0]  s_axis_tx_tdata,
    output reg [7:0]   s_axis_tx_tkeep,
    output reg 	       s_axis_tx_tlast,
    input wire 	       s_axis_tx_tready,
    output reg 	       s_axis_tx_tvalid,
    output reg [3:0]   s_axis_tx_tuser,

    // Configuration bits.
    output reg [15:0]  cfg_command,
    output wire [15:0] cfg_dcommand,
    output wire [15:0] cfg_dstatus,
    output wire [15:0] cfg_lcommand,
    output wire [15:0] cfg_lstatus,
    output reg [7:0]   cfg_bus_number,
    output reg [4:0]   cfg_dev_number,
    output reg [2:0]   cfg_fun_number,

    // Interupt management
    input wire 	       cfg_interrupt,
    output reg 	       cfg_interrupt_rdy,
    input wire 	       cfg_interrupt_assert,
    input wire [7:0]   cfg_interrupt_di,
    output reg 	       cfg_interrupt_msienable,
    output reg 	       cfg_interrupt_msixenable,

    // Errors
    output reg 	       cfg_err_cpl_rdy,

    // Various
    input wire 	       cfg_trn_pending
    /* */);

   reg [31:0]  cfg_mem[0 : 1023];

   // Masks for the bar registers. This is fairly raw, and tells us
   // which bits are writable.
   reg [31:0]  bar_mask_reg[4:9];

   // Calculated base addresses. These may be spread over multiple
   // BAR registers for 64bit addresses. The bar_addr is the assembled
   // address (64bits) for the bar, and the bar_map maps the BAR register
   // to the bar_addr that is applies to. The bar_map[x][3] bit means
   // the bar points to the high 32bits of a bar_addr, and bar_map[x][2:0]
   // points to the bar_addr that is the target.
   // The bar_hit_mask is a mask of which BARs are "hit" if the BAR is
   // addressed. This information winds up going into the tuser stream
   // to indicate BAR decoding to the user.
   reg [63:0]  bar_addr[0:5];
   reg [63:0]  bar_addr_mask[0:5];
   reg [3:0]   bar_map[4:9];
   reg [5:0]   bar_hit_mask[0:5];

   // State for receiving a TLP.
   reg [31:0]  tlp [0:3];
   reg [7:0]   ntlp;
   reg 	       tlp_is_config, tlp_pass, tlp_is_32addr, tlp_is_64addr;
   wire        tlp_pass_drain;
   reg [5:0]   tlp_bar_hit;

   // TREADY signal from the buffer. The flow control through this module
   // relies on the buf as well as the config receiver itself.
   wire        m_axis_rx_tready_buf;
   reg 	       m_axis_rx_tready_int;
   assign m_axis_rx_tready = m_axis_rx_tready_buf & m_axis_rx_tready_int;

   // State for transmitting a completion TLP.
   reg [63:0]  cmp_data[0:1];
   reg [7:0]   cmp_keep[0:1];
   reg [7:0]   ncmp, cmp_cur;

   // Expose some registers through teh cfg_ interface
   assign {cfg_dstatus, cfg_dcommand} = cfg_mem['h64/4];
   assign {cfg_lstatus, cfg_lcommand} = cfg_mem['h70/4];

   // The dstatus[5] bit tracks the trn_pending signal.
   always @(cfg_trn_pending) cfg_mem['h64/4][16+5] = cfg_trn_pending;

   // Stub the completion error machine to be always ready.
   always @(posedge user_clk) cfg_err_cpl_rdy <= 1;

   // Configuration writes are sometimes non-trivial, because not all
   // registers, and in some cases not all bits, are necessarily writable.
   // This task figures all that out and causes the correct memory bits
   // to be written.
   task do_cfg_write(input [11:2]adr, input [31:0]val, input [3:0]ben);
      localparam [31:0] CMD_STATUS_MASK     = 32'h0000_0507;
      localparam [31:0] DEV_CMD_STATUS_MASK = 32'h0000_7fff;
      localparam [31:0] LNK_CMD_STATUS_MASK = 32'h0000_0ffb;
      reg [31:0] mask;
      begin
	 mask[31:24] = ben[3]? 8'hff : 8'h00;
	 mask[23:16] = ben[2]? 8'hff : 8'h00;
	 mask[15: 8] = ben[1]? 8'hff : 8'h00;
	 mask[ 7: 0] = ben[0]? 8'hff : 8'h00;
	 case (adr)
	   0: begin
	   end
	   // {Status/command} registers
	   1: begin
	      cfg_mem[1] = val&(CMD_STATUS_MASK&mask) | cfg_mem[1]&~(CMD_STATUS_MASK&mask);
	      cfg_command <= cfg_mem[1][15:0];
	   end
	   // BARs can only have some of their bits written.
	   4,5,6,7,8,9: begin
	      mask = mask & bar_mask_reg[adr];
	      cfg_mem[adr] = val&mask | cfg_mem[adr]&~mask;
	      if (bar_map[adr][3])
		bar_addr[ bar_map[adr][2:0] ][63:32] = cfg_mem[adr];
	      else
		bar_addr[ bar_map[adr][2:0] ][31:4] = cfg_mem[adr][31:4];
	   end
	   // Device status/command registers
	   'h64 / 4: begin
	      cfg_mem['h64/4] = val&(DEV_CMD_STATUS_MASK&mask) | cfg_mem['h64/4]&~(DEV_CMD_STATUS_MASK&mask);
	   end
	   // Link Status/Control registers
	   'h70 / 4: begin
	      cfg_mem['h70/4] = val&(LNK_CMD_STATUS_MASK&mask) | cfg_mem['h70/4]&~(LNK_CMD_STATUS_MASK&mask);
	   end

	   default: begin
	      cfg_mem[adr] = val&mask | cfg_mem[adr]&~mask;
	   end
	 endcase
      end
   endtask // do_cfg_write

   // A configuration TLP is finished, so process it and make
   // a completion.
   task make_completion;
      reg [7:0]  tlp_tag;
      reg [11:2] tlp_adr;
      reg [31:0] tlp_val;
      reg [3:0]  tlp_ben;
      reg [7:0]  tlp_bus;
      reg [4:0]  tlp_dev;
      reg [2:0]  tlp_fun;
      begin
	 if (ncmp != 0) begin
	    $display("%m: ERROR: Completion buffer overrun?!");
	    $finish(1);
	 end
	 // Extract parts that we will need to make up the completion
	 tlp_tag = tlp[1][15:8];
	 tlp_ben = tlp[1][3:0];
	 tlp_bus = tlp[2][31:24];
	 tlp_dev = tlp[2][23:19];
	 tlp_fun = tlp[2][18:16];
	 tlp_adr = tlp[2][11:2];
	 tlp_val = ntlp==4? tlp[3] : 32'h00000000;
	 // This is how we find out our bus/dev/fn id.
	 cfg_bus_number <= tlp_bus;
	 cfg_dev_number <= tlp_dev;
	 cfg_fun_number <= tlp_fun;

	 // Generate the completion based on the request.
	 case (tlp[0][31:24])
	   'b000_00100: begin // CfgRd0
	      $display("%m: Got a CfgRd0 (tag=%h, adr=%h, , ben=%b, val=%h)",
		       tlp_tag, tlp_adr, tlp_ben, cfg_mem[tlp_adr]);
	      cmp_data[0] = {tlp_bus, tlp_dev, tlp_fun, 32'h4a000001};
	      cmp_data[1] = {cfg_mem[tlp_adr], 16'h0000, tlp_tag, 8'h00};
	      cmp_keep[0] = 8'hff;
	      cmp_keep[1] = 8'hff;
	      ncmp <= 2;
	      cmp_cur <= 0;
	   end

	   'b010_00100: begin // CfgWr0
	      $display("%m: Got a CfgWr0 (tag=%h, adr-%h, ben=%b val=%h)", tlp_tag, tlp_adr, tlp_ben, tlp_val);
	      do_cfg_write(tlp_adr, tlp_val, tlp_ben);
	      cmp_data[0] = {tlp_bus, tlp_dev, tlp_fun, 32'h0a000000};
	      cmp_data[1] = {32'h00000000, 16'h0000, tlp_tag, 8'h00};
	      cmp_keep[0] = 8'hff;
	      cmp_keep[1] = 8'h0f;
	      ncmp <= 2;
	      cmp_cur <= 0;
	   end

	   default: begin // Unknown or unsupported
	   end
	 endcase // case (tlp[0][31:24])
      end
   endtask

   task collect_tlp_words;

      reg [3:0] idx, nbyte, keep_bit;
      reg [31:0] val;

      begin
      	 nbyte = 0;
	 for (idx = 0 ; idx < 8 ; idx = idx+1) begin
	    keep_bit = {idx[2], 2'd3-idx[1:0]};
	    if (m_axis_rx_tkeep[keep_bit]) begin
	       val = {val[23:0], m_axis_rx_tdata[8*keep_bit +: 8]};
	       nbyte = nbyte+1;
	       if (nbyte == 4) begin
		  tlp[ntlp] = val;
		  ntlp = ntlp+1;
		  nbyte = 0;
	       end
	    end
	 end

	 // The remote probably always sends an even number of TLP words
	 // in each beat of the AXI4Stream. It is theoretically possible,
	 // but I don't the the Xilinx PCIe core does that.
	 if (nbyte != 0) begin
	    $display("%m: ERROR: I don't know how to handle odd bytes in tdata!");
	    $finish(1);
	 end
      end
   endtask // if

   always @(posedge user_clk) begin : tlp_in_block

      reg  [3:0] idx;
      reg [63:0] tmp_addr;
      reg [5:0]  tmp_bar_hit;

      if (user_reset) begin
	 ntlp <= 0;
	 tlp_is_config <= 0;
	 tlp_pass      <= 0;
	 tlp_is_32addr <= 0;
	 tlp_is_64addr <= 0;
	 tlp_bar_hit   <= 6'b000000;
	 m_axis_rx_tready_int <= 1;

	 cfg_command <= 0;
	 cfg_bus_number <= 0;
	 cfg_dev_number <= 0;
	 cfg_fun_number <= 0;
	 cfg_interrupt_msienable <= 0;
	 cfg_interrupt_msixenable <= 0;

      end else if (tlp_is_32addr) begin // if (user_reset)

	 if (m_axis_rx_tready && m_axis_rx_tvalid)
	   collect_tlp_words;

	 if (ntlp >= 3) begin
	    tmp_addr[63:32] = 0;
	    tmp_addr[31: 0] = tlp[2];
	    tmp_bar_hit = 0;
	    for (idx = 0 ; idx < 6 ; idx = idx+1) begin
	       //$display("%m: tmp_addr=%h, bar_addr[%0d]=%h, bar_addr_mask=%h",
	       //	  tmp_addr, idx, bar_addr[idx], bar_addr_mask[idx]);
	       if (bar_addr[idx] && (tmp_addr&bar_addr_mask[idx])==bar_addr[idx])
		 tmp_bar_hit = tmp_bar_hit | bar_hit_mask[idx];
	    end
	    tlp_is_32addr <= 0;
	    tlp_pass <= 1;
	    tlp_bar_hit <= tmp_bar_hit;
	    ntlp = 0;
	 end

      end else if (tlp_is_64addr) begin // if (user_reset)

	 if (m_axis_rx_tready && m_axis_rx_tvalid)
	   collect_tlp_words;

	 if (ntlp >= 4) begin
	    tmp_addr[63:32] = tlp[2];
	    tmp_addr[31: 0] = tlp[3];
	    tmp_bar_hit = 0;
	    for (idx = 0 ; idx < 6 ; idx = idx+1) begin
	      if (bar_addr[idx] && (tmp_addr&bar_addr_mask[idx])==bar_addr[idx])
		tmp_bar_hit = tmp_bar_hit | bar_hit_mask[idx];
	    end
	    tlp_is_64addr <= 0;
	    tlp_pass <= 1;
	    tlp_bar_hit <= tmp_bar_hit;
	    ntlp = 0;
	 end

      end else if (tlp_pass | tlp_pass_drain) begin // if (tlp_is_64addr)
	 // The buffer has received the tlp_pass flag, so reset the state
	 // machine and wait for the TLP to complete.
	 if (tlp_pass & tlp_pass_drain) begin
	    tlp_pass    <= 0;
	    tlp_bar_hit <= 6'b000000;
	    ntlp = 0;
	 end

	 // NOTE: The buffer should be holding the tready low to prevent
	 // new TLPs from starting while it is draining the curreht TLP.
	 // That means we don't need to be looking at the input stream
	 // while we are in pass mode.
	 
      end else if (tlp_is_config) begin // if (tlp_pass | tlp_pass_drain)
	 // Collect the next word.
	 collect_tlp_words;

	 // If this is the last word of the config TLP, make the completion.
	 if (m_axis_rx_tlast) begin
	    // Now we have a Config TLP, process it by forming a completion.
	    //$display("%m: Got a Config TLP: ntlp=%0d", ntlp);
	    make_completion;
	   
	    // Done with the TLP, clear the rx state machine
	    ntlp = 0;
	    tlp_is_config <= 0;
	    tlp_pass      <= 0;
	    tlp_bar_hit   <= 6'b000000;
	 end // if (m_axis_rx_tlast)

      end else if (m_axis_rx_tready && m_axis_rx_tvalid) begin
	 // If this TLP is not yet identified...
	 
	 // Extract the bytes of the TLP from the word.
	 collect_tlp_words;

	 // If this is the first word of the TLP, then check if it is a
	 // a config. If it is, then set a flag so that we continue to
	 // capture the tlp. Otherwise, set a different flag so that we
	 // know to skip the rest of this TLP.
	 case (tlp[0][31:24])
	   'b000_00100: tlp_is_config <= 1; // CfgRd0
	   'b010_00100: tlp_is_config <= 1; // CfgWr0
	   'b000_00101: tlp_is_config <= 1; // CfgRd1
	   'b010_00101: tlp_is_config <= 1; // CfgWr1
	   'b000_00000: tlp_is_32addr <= 1; // Rd32
	   'b010_00000: tlp_is_32addr <= 1; // Wr32
	   'b001_00000: tlp_is_64addr <= 1; // Rd64
	   'b011_00000: tlp_is_64addr <= 1; // Wr64
	   default:     tlp_pass      <= 1;
	 endcase
      
	 if (m_axis_rx_tlast) begin
	    $display("%m: ERROR: First word is last word? tlp[0] is %h", tlp[0]);
	    $finish;
	 end

      end else begin
	
      end
   end

   task drive_cmp_word(input reg [7:0] idx);
      begin
	 s_axis_tx_tdata  <= cmp_data[idx];
	 s_axis_tx_tkeep  <= cmp_keep[idx];
	 s_axis_tx_tlast  <= (idx+1) == ncmp;
	 s_axis_tx_tvalid <= 1;
      end
   endtask // drive_cmp_word

   always @(posedge user_clk) begin : cmp_machine

      reg [15:0] val16;
      if (user_reset) begin
	 cmp_cur <= 0;
	 ncmp    <= 0;
	 s_axis_tx_tvalid <= 0;
	 s_axis_tx_tuser  <= 0;
	 tx_cfg_req       <= 0;

      end else if (s_axis_tx_tready & s_axis_tx_tvalid) begin
	 // If the remote consumes a word, set up the next word. If the
	 // previous word was the last (tlast) then clear the cmp buffer
	 // and the stream signals. Otherwise, drive the next word and
	 // increment the transmit count.
	 if (s_axis_tx_tlast) begin
	    s_axis_tx_tvalid <= 0;
	    s_axis_tx_tlast  <= 0;
	    cmp_cur <= 0;
	    ncmp    <= 0;
	    tx_cfg_req <= 0;

	 end else begin
	    drive_cmp_word(cmp_cur);
	    cmp_cur <= cmp_cur+1;
	 end

      end else if (s_axis_tx_tvalid==0 && ncmp!=0) begin
	 // Is there a completion waiting to be started?

	 // Request the transmit stream, if not requested already.
	 if (tx_cfg_req == 0)
	   tx_cfg_req <= 1;
	 // Drive the first word of the completion TLP. The remaining
	 // words will be handled in another condition.
	 if (tx_cfg_gnt) begin
	    drive_cmp_word(0);
	    cmp_cur <= 1;
	 end

      end else begin
      end
   end // block: cmp_machine

   // This machine takes in the interrupt generation requests from the
   // user code and generates the messages needed to transmit the
   // right controls to the root.
   always @(posedge user_clk) begin : interrupts_machine
      reg [7:0] code;

      if (user_reset) begin
	 cfg_interrupt_rdy <= 0;

      end else if (cfg_interrupt & cfg_interrupt_rdy) begin
	 cfg_interrupt_rdy <= 0;

      end else if (cfg_interrupt & ncmp==0) begin

	 if (cfg_command[10] & cfg_interrupt_assert) begin
	    $display("%m: ERROR: User trying to assert legacy interrupt while global interrupts blocked.");
	    $display("%m:      : cfg_command=%h", cfg_command);
	 end

	 // Make a Message Request TLP to send the assert/deassert message
	 // to the root. When that is done, we tell the user that we have
	 // received the interrupt command. The transmit will happen at
	 // its own pace.
	 if (cfg_interrupt_assert)
	   code = 8'h20;
	 else
	   code = 8'h24;
	 cmp_data[0] <= {24'h000000, code, 32'h30_000000};
	 cmp_keep[0] <= 8'b1111_1111;
	 cmp_data[1] <= {64'h00000000_00000000};
	 cmp_keep[1] <= 8'b1111_1111;
	 ncmp <= 2;
	 cfg_interrupt_rdy <= 1;

      end else begin
      end
   end

   xilinx_pcie_rx_buffer buffer
     (.user_clk(user_clk),
      .user_reset(user_reset),

      .tlp_pass(tlp_pass),
      .tlp_pass_drain(tlp_pass_drain),
      .tlp_drop(tlp_is_config),
      .tlp_bar_hit(tlp_bar_hit),

      .i_axis_rx_tdata(m_axis_rx_tdata),
      .i_axis_rx_tkeep(m_axis_rx_tkeep),
      .i_axis_rx_tlast(m_axis_rx_tlast),
      .i_axis_rx_tready(m_axis_rx_tready_buf),
      .i_axis_rx_tvalid(m_axis_rx_tvalid),

      .o_axis_rx_tdata(o_axis_rx_tdata),
      .o_axis_rx_tkeep(o_axis_rx_tkeep),
      .o_axis_rx_tlast(o_axis_rx_tlast),
      .o_axis_rx_tready(o_axis_rx_tready),
      .o_axis_rx_tvalid(o_axis_rx_tvalid),
      .o_axis_rx_tuser(o_axis_rx_tuser)
      /* */);

   // Pre-reset initialization
   initial begin : init
      integer rc, bdx;
      reg [15:0] val16;
      reg [31:0] val32;
      reg 	 flag64;

      // Initialize the config space.
      rc = $sscanf(ven_id, "%h", val16);
      cfg_mem[0][15:0] = val16;
      rc = $sscanf(dev_id, "%h", val16);
      cfg_mem[0][31:16] = val16;

      rc = $sscanf(bar_0, "%h", val32);
      cfg_mem[4] = val32;
      bar_mask_reg[4] = val32 & 32'hfffffff0;
      if (val32[2:1] == 2'b10) flag64 = 1;
      else flag64 = 0;

      rc = $sscanf(bar_1, "%h", val32);
      cfg_mem[5] = val32;
      if (flag64) begin
	 bar_mask_reg[5] = 32'hffffffff;
	 flag64 = 0;
      end else begin
	 bar_mask_reg[5] = val32 & 32'hfffffff0;
	 if (val32[2:1] == 2'b10) flag64 = 1;
      end

      rc = $sscanf(bar_2, "%h", val32);
      cfg_mem[6] = val32;
      if (flag64) begin
	 bar_mask_reg[6] = 32'hffffffff;
	 flag64 = 0;
      end else begin
	 bar_mask_reg[6] = val32 & 32'hfffffff0;
	 if (val32[2:1] == 2'b10) flag64 = 1;
      end

      rc = $sscanf(bar_3, "%h", val32);
      cfg_mem[7] = val32;
      if (flag64) begin
	 bar_mask_reg[7] = 32'hffffffff;
	 flag64 = 0;
      end else begin
	 bar_mask_reg[7] = val32 & 32'hfffffff0;
	 if (val32[2:1] == 2'b10) flag64 = 1;
      end

      rc = $sscanf(bar_4, "%h", val32);
      cfg_mem[8] = val32;
      if (flag64) begin
	 bar_mask_reg[8] = 32'hffffffff;
	 flag64 = 0;
      end else begin
	 bar_mask_reg[8] = val32 & 32'hfffffff0;
	 if (val32[2:1] == 2'b10) flag64 = 1;
      end

      rc = $sscanf(bar_5, "%h", val32);
      cfg_mem[9] = val32;
      if (flag64) begin
	 bar_mask_reg[9] = 32'hffffffff;
	 flag64 = 0;
      end else begin
	 bar_mask_reg[9] = val32 & 32'hfffffff0;
	 if (val32[2:1] == 2'b10) flag64 = 1;
      end

      rc = $sscanf(subsys_ven_id, "%h", val16);
      cfg_mem[11][15:0] = val16;
      rc = $sscanf(subsys_id, "%h", val16);
      cfg_mem[11][31:16] = val16;
      rc = $sscanf(xrom_bar, "%h", val32);
      cfg_mem[12] = val32;
      $display("%m: Initialize config space:");
      for (rc = 0 ; rc < 16 ; rc = rc+4)
	$display("%m: %h: %h %h %h %h", rc[7:0], cfg_mem[rc+0], cfg_mem[rc+1], cfg_mem[rc+2], cfg_mem[rc+3]);

      rc = 4;
      for (bdx = 0 ; bdx < 6 ; bdx = bdx+1) begin
	 bar_addr[bdx] = 0;
	 bar_hit_mask[bdx] = 6'b000000;
	 if (rc <= 9) begin
	    bar_map[rc] = bdx;
	    bar_addr_mask[bdx][63:32] = 32'hffffffff;
	    bar_addr_mask[bdx][31: 0] = {cfg_mem[rc][31:4], 4'b0000};
	    bar_hit_mask[bdx][rc-4] = 1'b1;
	    if (cfg_mem[rc][2:1] == 2'b10) begin
	       bar_map[rc+1] = bdx | 'b1_000;
	       bar_hit_mask[bdx][rc-4+1] = 1'b1;
	       rc = rc+2;
	    end else begin
	       rc = rc+1;
	    end
	 end
      end
   end

endmodule // xilinx_pcie_cfg_space

/*
 * This module captures and buffers a TLP from the remote. Once it is
 * determined by the context that the incoming TLP is to be passed
 * through, it starts directing it through to the output. If it is
 * determinted that the TLP is to be consumed, then it is ignored
 * and dropped.
 */
module xilinx_pcie_rx_buffer
  (/* */
   input wire 	     user_clk,
   input wire 	     user_reset,

   input wire 	     tlp_pass,
   output reg 	     tlp_pass_drain,
   input wire 	     tlp_drop,
   input wire [5:0]  tlp_bar_hit,

   // Receive channel AXI4 Stream (master side)
   input wire [63:0] i_axis_rx_tdata,
   input wire [7:0]  i_axis_rx_tkeep,
   input wire 	     i_axis_rx_tlast,
   output reg 	     i_axis_rx_tready,
   input wire 	     i_axis_rx_tvalid,

    // Receive channel AXI4 Stream (master side)
   output reg [63:0] o_axis_rx_tdata,
   output reg [7:0]  o_axis_rx_tkeep,
   output reg 	     o_axis_rx_tlast,
   input wire 	     o_axis_rx_tready,
   output reg 	     o_axis_rx_tvalid,
   output reg [21:0] o_axis_rx_tuser
   /* */);

   reg [63:0] 	     tdata_buf[0:3];
   reg [7:0] 	     tkeep_buf[0:3];
   reg 		     tlast_buf[0:3];

   reg [1:0] 	     ptr;
   reg [2:0] 	     fill;
   reg [5:0] 	     sav_bar_hit;

   task push_beat;
      reg [1:0] nxt;
      begin
	 nxt = ptr + fill;
	 tdata_buf[nxt] <= i_axis_rx_tdata;
	 tkeep_buf[nxt] <= i_axis_rx_tkeep;
	 tlast_buf[nxt] <= i_axis_rx_tlast;
	 fill = fill + 1;
      end
   endtask // always

   task pull_beat;
      reg [5:0] tmp_bar_hit;
      begin
	 if (tlp_pass)
	   tmp_bar_hit = tlp_bar_hit;
	 else
	   tmp_bar_hit = sav_bar_hit;
	 o_axis_rx_tdata  <= tdata_buf[ptr];
	 o_axis_rx_tkeep  <= tkeep_buf[ptr];
	 o_axis_rx_tlast  <= tlast_buf[ptr];
	 o_axis_rx_tvalid <= 1;
	 o_axis_rx_tuser  <= {14'b0, tmp_bar_hit, 2'b00};
	 ptr = ptr+1;
	 fill = fill - 1;
      end
   endtask // always


   always @(posedge user_clk)
     if (user_reset) begin
	ptr  = 0;
	fill = 0;
	tlp_pass_drain <= 0;
	sav_bar_hit    <= 0;

	i_axis_rx_tready <= 1;

	o_axis_rx_tlast  <= 0;
	o_axis_rx_tvalid <= 0;

     end else if (tlp_drop) begin
	// If we are in drop mode, then forget the beats that
	// we collected so far, and ignore the remaining until
	// we exit from drop mode.
	ptr  = 0;
	fill = 0;

     end else if (tlp_pass | tlp_pass_drain) begin
	// If we are in pass mode, then clock the TLP out to the
	// destination as quickly as it will go.

	if (o_axis_rx_tready && o_axis_rx_tvalid && o_axis_rx_tlast) begin
	   // If we output the last word of the TLP, then the pass
	   // is done and we can turn the tready back on.
	   i_axis_rx_tready <= 1;
	   tlp_pass_drain  <= 0;

	end else begin
	   // If this is NOT the last, then continue draining.
	   tlp_pass_drain <= 1;
	end

	// The tlp_bar_hit is only valid during the tlp_pass signal.
	if (tlp_pass) sav_bar_hit <= tlp_bar_hit;

	if (fill>0 && o_axis_rx_tready && o_axis_rx_tvalid)
	  // Data waiting, and current word is consumed...
	  pull_beat;
	else if (fill>0 && !o_axis_rx_tvalid)
	  // Data waiting, and no current word driven...
	  pull_beat;
	else if (o_axis_rx_tready && o_axis_rx_tvalid && o_axis_rx_tlast) begin
	   // Last word has been consumed.
	   o_axis_rx_tvalid <= 0;
	   o_axis_rx_tlast  <= 0;
	   o_axis_rx_tdata  <= 64'bx;
	   o_axis_rx_tkeep  <= 8'bx;
	   o_axis_rx_tuser  <= 22'b0;
	end

	// In general, if we are passing the TLP, then push the TLP
	// stream work onto the queue. If this is the last, then mark
	// the input tready so that no new words come in until the
	// drain completes.
	if (i_axis_rx_tready & i_axis_rx_tvalid) begin
	   push_beat;
	   if (i_axis_rx_tlast)
	     i_axis_rx_tready <= 0;
	end

     end else if (i_axis_rx_tready & i_axis_rx_tvalid) begin
	// Here, we don't know yet if the current TLP is to be dropped
	// or passed. So keep saving it.
	push_beat;

	// don't take in any more TLPs until we know what to do
	// with the one we just captured.
	if (i_axis_rx_tlast) i_axis_rx_tready <= 0;

     end

endmodule // xilinx_pcie_rx_buffer
