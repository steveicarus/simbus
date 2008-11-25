
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

/* Runtime arguments:
 *
 *    +bar0-mask=%h         -- Set the BAR0 mask. This defines the size of
 *                             the memory region. The default is fffff000 (4K).
 *
 *    +mem-retry-rate=%d
 *    +mem-discon-rate=%d
 *                          -- Set rates that the memory will retry commands
 *                             and disconnect bursts. The default is 0 (off).
 */

/*
 * The pci memdev device is a virtual memory device that acts like an
 * ordinary memory in the region of BAR0. There are magical control
 * registers in BAR2.
 *
 * BAR0 is the memory region. The BAR is 64bit, with the upper 32bits
 * consuming what would normally be BAR1.
 *
 * BAR1 is the upper 32bits of the address for the memory region.
 * 
 * BAR2 is a command register set. It is 4K long, and used to carry
 * commands that the memory device itself can perform. Word-0 is the
 * command register, and writes to that address execute the specified
 * command. The next 4095 words are the arguments or results of the
 * command.
 * 
 * The BAR2 commands are as follows:
 * 
 * FILL: 0x00000000 <base> <size> <fill>
 * 
 *    The fill command causes the memory starting at <base> (the offset
 *    within the memory region) and for <size> words to be filled with
 *    the word value <fill>
 * 
 * COMPARE: 0x00000001 <base1> <size1> <base2>
 * 
 *    The COMPARE command causes the memory to compare two regions of
 *    memory, <base1> and <base2>. The <size1> is the size in bytes of
 *    the regions.
 * 
 *    The result is as memcmp, and the result code replaces the command
 *    word.
 *
 * LOAD: 0x00000002 <base> <size> <file name...>
 * 
 *    The LOAD command reads host file contents into the address at
 *    <base> and for <size> bytes. The actual number of bytes loaded
 *    is returned in the command word. If there is an error reading
 *    the file, a -1 (0xffffffff) is returned instead.
 * 
 *    The file name is a null terminated string of bytes. The name
 *    is interpreted by the host system.
 *
 * SAVE: 0x00000003 <base> <size> <file name...>
 * 
 *    The SAVE command writes a host file with the contents of memory
 *    at offset <base> and for <size> bytes.
 *
 * After the command region, The BAR2 region has registers of a DMA
 * controller. The DMA controller supports a single linear read.
 *
 *    0x1000  : Cmd/Status
 *       [0] GO
 *       [1] FIFO Mode (0 - linear external addresses, 1 - fixed address)
 *       [2] Enable Count Interrupt
 *       [3] DAC64 addresses
 *
 *    0x1008  : External Address (low 32 bits)
 *    0x100c  : External Address (high 32 bits, defaults to 0)
 *
 *    0x1010  : Memory Offset
 *
 *    0x1004  : Transfer count
 */

`timescale 1ns/1ns

module main;

   wire clk, reset_n, idsel;
   wire gnt_n, req_n;
   wire inta;

   wire        frame_n, irdy_n, trdy_n, stop_n, devsel_n;
   wire [3:0]  c_be_h, c_be_l;
   wire [7:0]  c_be;
   wire [63:0] ad;
   wire        par;

   pci_slot #(.name("pcimem")) slot
     (.PCI_CLK(clk),
      .RESET_n(reset_n),
      .IDSEL(idsel),
      .GNT_n(gnt_n),
      .REQ_n(req_n),
      .INTA_n(~inta),
      .INTB_n(1'b1),
      .INTC_n(1'b1),
      .INTD_n(1'b1),
      .FRAME_n(frame_n),
      .IRDY_n(irdy_n),
      .TRDY_n(trdy_n),
      .STOP_n(stop_n),
      .DEVSEL_n(devsel_n),
      .C_BE(c_be),
      .AD(ad),
      .PAR(par)
      /* */);

   pci64_memory dev
     (.CLK(clk),
      .RESET(reset_n),
      .IDSEL(idsel),
      .REQ(req_n),
      .GNT(gnt_n),
      .INTA(inta),
      
      .FRAME(frame_n),
      .IRDY(irdy_n),
      .TRDY(trdy_n),
      .STOP(stop_n),
      .DEVSEL(devsel_n),

      .C_BE(c_be[3:0]),
      .AD(ad[31:0]),
      .PAR(par)
      /* */);

   initial begin
      $dumpfile("pcimem.lxt");
      $dumpvars(0);
   end

endmodule // main

module pci64_memory (CLK, RESET, AD, C_BE, PAR,
		     FRAME, TRDY, IRDY, STOP,
		     DEVSEL, IDSEL, REQ, GNT, INTA);

   // This is the device ID of this PCI device.
   parameter DEVICE_ID  = 32'hffc1_12c5;

   // Use this to set the size of the memory region. Even though
   // memory device understands 64bit addresses, keep the size
   // to <= 4Gig.
   reg [31:0] bar0_mask;

   // Memories are normally prefetchable. (Prefetchable=1)
   // This memory can go anywhere is 64bit address space. (Type=10)
   parameter BAR0_FLAGS = 'hc;

   // This is the default memory image file. override this
   // parameter to use a different host file.
   parameter IMAGE      = "memory.bin";

   // The command bar (BAR2) is fixed size.
   localparam BAR2_MASK  = 32'hffffe000;
   localparam BAR2_FLAGS = 'hc;

   input CLK;
   input RESET;

   inout [31:0] AD;
   inout [3:0] 	C_BE;
   inout        PAR;

   inout 	FRAME, TRDY, IRDY, STOP;
   inout 	DEVSEL;
   input 	IDSEL;

   output 	INTA;
   wire 	INTA = (dma_count == 0) && dma_count_intr_en;
   output 	REQ;
   input 	GNT;
   reg 		REQ = 1;

   reg 	      PAR_reg;
   reg 	      PAR_en;
   assign     PAR = PAR_en? PAR_reg : 1'bz;

   reg 	      FRAME_reg = 1;
   reg 	      FRAME_en = 0;
   assign     FRAME = FRAME_en? FRAME_reg : 1'bz;

   reg 	      TRDY_reg;
   reg 	      TRDY_en = 0;
   assign     TRDY = TRDY_en? TRDY_reg : 1'bz;

   reg 	      IRDY_reg = 1;
   reg 	      IRDY_en = 0;
   assign     IRDY = IRDY_en? IRDY_reg : 1'bz;

   reg 	      STOP_reg;
   reg 	      STOP_en = 0;
   assign     STOP = STOP_en? STOP_reg : 1'bz;

   reg 	      DEVSEL_reg;
   reg 	      DEVSEL_en;
   assign     DEVSEL = DEVSEL_en? DEVSEL_reg : 1'bz;

   reg [3:0] 	C_BE_reg;
   reg 		C_BE_en = 0;
   bufif1 c_be_buf[3:0](C_BE, C_BE_reg, C_BE_en);

   reg [31:0] 	AD_reg;
   reg 		AD_en = 0;
   bufif1 ad_buf[31:0](AD, AD_reg, AD_en);

   // This holds the configuration space of the device. Only the
   // first 16 words are implemented.
   reg [31:0] 	config_mem[15:0];

   // Registers related to DMA (BAR4)
   reg 		dma_go = 0;
   reg 		dma_fifo = 1;
   reg 		dma_count_intr_en = 0;
   reg 		dma_dac64 = 0; 		
   reg [31:0] 	dma_external_l, dma_external_h;
   reg [31:0] 	dma_memory;
   reg [31:0] 	dma_count;

   // This function returns true if the input address addresses
   // this device. Use BAR0 and the BAR mask to decode the address.
   function [0:0] selected;
      input [63:0] addr;
      if ((config_mem[4] & bar0_mask) == 0 && config_mem[5] == 0)
	selected = 0;
      else if (config_mem[5] != addr[63:32])
	selected = 0;
      else if ((addr[31:0] & bar0_mask) == (config_mem[4] & bar0_mask))
	selected = 1;
      else
	selected = 0;
   endfunction

   // This checks if BAR2 has been selected. We handle different
   // things here.
   function [0:0] selected1;
      input [63:0] addr;
      if ((config_mem[6] & BAR2_MASK) == 0 && (config_mem[7] == 0))
	selected1 = 0;
      else if (config_mem[7] != addr[63:32])
	selected1 = 0;
      else if ((addr[31:0] & BAR2_MASK) == (config_mem[6] & BAR2_MASK) && addr[12]==0)
	selected1 = 1;
      else
	selected1 = 0;
   endfunction

   // This checks if BAR2 DMA has been selected. 
   function [0:0] selected2;
      input [63:0] addr;
      if ((config_mem[6] & BAR2_MASK) == 0 && (config_mem[7] == 0))
	selected2 = 0;
      else if (config_mem[7] != addr[63:32])
	selected2 = 0;
      else if ((addr[31:0] & BAR2_MASK) == (config_mem[6] & BAR2_MASK) && addr[12]==1)
	selected2 = 1;
      else
	selected2 = 0;
   endfunction


   // handle for accessing the memory file.
   integer 	   memory_fd;
 	   
   // Address that is collected from the address phase
   // of a PCI transaction.
   reg [63:0] 	   use_address;
   reg [31:0] 	   write_mask;
   reg [31:0] 	   write_val;

   reg pending_flag = 0;

   // These parameters control the memory device' tendency to issue a
   // retry on a transaction.
   reg [31:0] retry_rate = 0;
   reg [31:0] discon_rate = 0;

   // Handle a PCI reset by resetting drivers and config space.
   task do_reset;
      begin
	 pending_flag = 0;
	 AD_reg = 0;
	 AD_en = 0;
	 C_BE_en = 0;
	 FRAME_en = 0;
	 IRDY_en = 0;
	 TRDY_en = 0;
	 STOP_en = 0;
	 DEVSEL_en = 0;
	 PAR_en = 0;

	 config_mem[ 0] = DEVICE_ID;
	 config_mem[ 1] = 32'h00000000;
	 config_mem[ 2] = 32'h05000000; /* RAM memory */
	 config_mem[ 3] = 32'h00000000;
	 config_mem[ 4] = 32'h00000000 | BAR0_FLAGS;
	 config_mem[ 5] = 32'h00000000;
	 config_mem[ 6] = 32'h00000000 | BAR2_FLAGS;
	 config_mem[ 7] = 32'h00000000;
	 config_mem[ 8] = 32'h00000000;
	 config_mem[ 9] = 32'h00000000;
	 config_mem[10] = 32'h00000000;
	 config_mem[11] = 32'h00000000;
	 config_mem[12] = 32'h00000000;
	 config_mem[13] = 32'h00000000;
	 config_mem[14] = 32'h00000000;
	 config_mem[15] = 32'h00000000;
      end
   endtask // do_reset

   task do_configuration_read;
      begin
	 AD_reg = config_mem[AD[9:2]];
	 TRDY_reg <= 0;
	 DEVSEL_reg <= 0;

	 @(posedge CLK) ;

	 AD_en <= 1;
	 TRDY_en <= 1;
	 DEVSEL_en <= 1;

	 @(posedge CLK) ;
	 // Parity bit is delayed one clock.
	 PAR_en <= 1;
	 PAR_reg <= ^{AD_reg, C_BE};

	 while (IRDY == 1)
	   @(posedge CLK) ;

	 TRDY_reg <= 1;
	 TRDY_en <= 0;
	 DEVSEL_reg <= 1;
	 DEVSEL_en <= 0;

	 @(posedge CLK) ;
	 AD_en <= 0;
	 PAR_en <= 0;
      end
   endtask // do_configuration_read

   task do_configuration_write;

      begin
	 // Activate TRDY and DEVSEL drivers, and start
	 // waiting for the IRDY.
	 TRDY_en <= 1;
	 DEVSEL_en <= 1;

	 // Even fast timing response requires that we wait.
	 @(posedge CLK) ;

	 // use_address was built up in address phase.
	 TRDY_reg <= 0;
	 DEVSEL_reg <= 0;

	 @(posedge CLK) /* Clock the TRDY and DEVSEL */ ;

	 while (IRDY == 1)
	   @(posedge CLK) ;

	 case (use_address[7:2])
	   6'b000000: /* device ID is read-only */;
	   6'b000010: /* class code is read-only */;
	   // Mask BAR0/1, and leave the other BARs zero.
	   6'b000100: config_mem[6'b000100] = AD[31:0] & bar0_mask | BAR0_FLAGS;
	   6'b000101: config_mem[6'b000101] = AD[31:0];
	   6'b000110: config_mem[6'b000110] = AD[31:0] & BAR2_MASK | BAR2_FLAGS;
	   6'b000111: config_mem[6'b000111] = AD[31:0];
	   6'b001000: /* BAR4 not implemented. */ ;
	   6'b001001: /* BAR5 not implemented. */ ;
	   6'b001010: /* CIS  not implemented. */ ;
	   6'b001011: /* subsystem id read-only */;
	   6'b001101: /* reserved */ ;
	   6'b001110: /* reserved */ ;

	   default:   config_mem[use_address[7:2]] = AD;

	 endcase // case(addr)

	 TRDY_reg <= 1;
	 TRDY_en <= 0;
	 STOP_reg <= 1;
	 STOP_en <= 0;
	 DEVSEL_reg <= 1;
	 DEVSEL_en <= 0;
      end
   endtask // do_configuration_write

   integer retry_tmp;
   reg [31:0] read_tmp;
   reg 	   parity_bit;
   task do_memory_read;
      input bar_flag;
      reg [31:0] masked_address;
      begin
	 masked_address = use_address[31:0] & ~bar0_mask;

	 // Fast timing, respond immediately
	 DEVSEL_reg <= 0;
	 DEVSEL_en  <= 1;

	 @(posedge CLK) ; // advance to turnaround cycle
	 retry_tmp = $random % retry_rate;
	 if ((retry_rate > 0) && ((retry_tmp) == 0)) begin
	    TRDY_reg <= 1;
	    STOP_reg <= 0;
	    TRDY_en <= 1;
	    STOP_en <= 1;

	    @(posedge CLK) ; // Drive STOP# for at least one CLK
	    
	    while (FRAME == 0)
	      @(posedge CLK) ; // Wait for the master to give up.

	    // Clean up the drivers.
	    TRDY_en <= 0;
	    STOP_en <= 0;
	    DEVSEL_en <= 0;
	    disable do_memory_read;
	 end

	 TRDY_reg <= 0;
	 TRDY_en  <= 1;
	 STOP_reg <= 1;
	 STOP_en  <= 1;
	 AD_en  <= 1;

	 // Read the addressed value,
	 read_tmp = $simbus_mem_peek(memory_fd, masked_address, bar_flag);
	 AD_reg  <= read_tmp;
	 parity_bit <= ^{read_tmp, C_BE};

	 // and go to the next address.
	 masked_address <= (masked_address + 4) & ~bar0_mask;

	 // Keep reading so long as the master does not
	 // stop the transaction. The TRDY_reg is there to
	 // catch the case that the frame goes away right
	 // away. (The TRDY_reg <= 0 above doesn't happen
	 // until the @posedge inside this loop.) The ~STOP
	 // prevents the TRDY_reg from a disconnect hanging
	 // this loop.
	 while ((TRDY_reg && STOP) || FRAME == 0)
	   @(posedge CLK) if ((IRDY == 0) && (TRDY == 0)) begin
	      PAR_reg <= parity_bit;
	      PAR_en  <= 1;
	      // Read the next value
	      read_tmp = $simbus_mem_peek(memory_fd, masked_address, bar_flag);
	      AD_reg  <= read_tmp;
	      parity_bit <= ^{read_tmp, C_BE};
	      // Linear addressing.
	      masked_address <= (masked_address + 4) & ~bar0_mask;

	      // If FRAME# is high, then this is the last word.
	      if (FRAME == 1)
		TRDY_reg <= 1;

	      // Randomly introduce target disconnects.
	      if (STOP_en && STOP_reg == 0) begin
		 TRDY_reg <= 1;
		 if (FRAME == 0 && TRDY_reg == 1) begin
		    $display("PCI MEMORY: master not honoring read disconnect!");
		    $display("PCI MEMORY: Forcing the issue via TRDY#=1.");
		    $display("PCI MEMORY: Stopping. Resume if you dare.");
		    $stop;
		 end

	      end else if (discon_rate && ($random%discon_rate == 0)) begin
		 STOP_reg <= 0;
	      end

	   end

	 // Transaction done. Turn off drivers.
	 TRDY_reg <= 1;
	 DEVSEL_reg <= 1;
	 STOP_reg <= 0;
	 AD_en <= 0;
	 TRDY_en <= 0;
	 DEVSEL_en <= 0;
	 STOP_en <= 0;
	 PAR_reg <= parity_bit;
	 PAR_en  <= 1;

	 @(posedge CLK)
	   PAR_en <= 0;
      end

   endtask // do_memory_read

   task do_memory_write;
      input bar_flag;
      reg [31:0] masked_address;
      begin
	 masked_address = use_address[31:0] & ~bar0_mask;

	 // Fast timing, respond immediately
	 DEVSEL_reg <= 0;
	 DEVSEL_en  <= 1;

	 // writes do not need a turnaround cycle, but throw in random
	 // waits anyhow. Give 'em a 50% chance.
	 if ($random%2 == 0) begin
	    TRDY_reg <= 1;
	    STOP_reg <= 1;
	    TRDY_en  <= 1;
	    STOP_en  <= 1;
	    @(posedge CLK) ;
	 end

	 // Now we're ready to receive data.
	 TRDY_reg <= 0;
	 STOP_reg <= 1;
	 TRDY_en  <= 1;
	 STOP_en  <= 1;

	 // Keep reading so long as the master does not
	 // stop the transaction.
	 while (FRAME == 0 || IRDY == 0)
	   @(posedge CLK) if ((IRDY == 0) && (TRDY == 0)) begin
	      write_mask = { {8{C_BE[3]}}, {8{C_BE[2]}},
			     {8{C_BE[1]}}, {8{C_BE[0]}}};
	      write_val = $simbus_mem_peek(memory_fd, masked_address, bar_flag);
	      write_val = (write_val&write_mask) | (AD&~write_mask);

	      // Store the next value
	      $simbus_mem_poke(memory_fd, masked_address, write_val, bar_flag);
	      // Linear addressing.
	      masked_address <= (masked_address + 4) & ~bar0_mask;

	      // If FRAME# is high, then this is the last word.
	      if (FRAME == 1)
		TRDY_reg <= 1;

	      // Randomly introduce target disconnects.
	      if (STOP_en && STOP_reg == 0) begin
		 TRDY_reg <= 1;
		 if (FRAME == 0 && TRDY_reg == 1) begin
		    $display("PCI MEMORY: master not honoring write disconnect!");
		    $display("PCI MEMORY: Forcing the issue via TRDY#=1.");
		    $display("PCI MEMORY: Stopping. Resume if you dare.");
		    $stop;
		 end

	      end else if (discon_rate && ($random%discon_rate == 0)) begin
		 STOP_reg <= 0;
	      end
	   end

	 // Transaction done. Turn off drivers.
	 TRDY_reg <= 1;
	 DEVSEL_reg <= 1;
	 TRDY_en <= 0;
	 DEVSEL_en <= 0;
	 STOP_reg <= 1;
	 STOP_en <= 0;
      end

   endtask // do_memory_write

   task do_bar2_read;
      begin
	 case (AD[11:2])
	   9'b000000000: AD_reg = {28'h0, dma_dac64, 1'b0, dma_fifo, dma_go};
	   9'b000000010: AD_reg = dma_external_l;
	   9'b000000011: AD_reg = dma_external_h;
	   9'b000000100: AD_reg = dma_memory;
	   9'b000000101: AD_reg = dma_count;
	   default: AD_reg = 32'hx;
	 endcase // case(AD[11:2])

	 TRDY_reg <= 0;
	 DEVSEL_reg <= 0;

	 @(posedge CLK) ;

	 AD_en <= 1;
	 TRDY_en <= 1;
	 DEVSEL_en <= 1;

	 @(posedge CLK) ;
	 // Parity bit is delayed one clock.
	 PAR_en <= 1;
	 PAR_reg <= ^{AD_reg, C_BE};

	 while (IRDY == 1)
	   @(posedge CLK) ;

	 TRDY_reg <= 1;
	 TRDY_en <= 0;
	 DEVSEL_reg <= 1;
	 DEVSEL_en <= 0;

	 @(posedge CLK) ;
	 AD_en <= 0;
	 PAR_en <= 0;
      end
   endtask // do_bar2_read

   task do_bar2_write;

      begin
	 // Activate TRDY and DEVSEL drivers, and start
	 // waiting for the IRDY.
	 TRDY_en <= 1;
	 DEVSEL_en <= 1;

	 // Even fast timing response requires that we wait.
	 @(posedge CLK) ;

	 // Save the address from the address phase.
	 TRDY_reg <= 0;
	 DEVSEL_reg <= 0;

	 @ (posedge CLK) /* Clock the TRDY# and DEVSEL# out */ ;

	 while (IRDY == 1)
	   @(posedge CLK) ;

	 case (use_address[11:2])
	   9'b000000000: begin
	      dma_go = AD[0];
	      dma_fifo = AD[1];
	      dma_count_intr_en = AD[2];
	      dma_dac64 = AD[3];
	   end
	   9'b000000010: dma_external_l = AD;
	   9'b000000011: dma_external_h = AD;
	   9'b000000100: dma_memory   = AD;
	   9'b000000101: dma_count    = AD;
	   default: ;
	 endcase // case(address[11:2])

	 TRDY_reg <= 1;
	 TRDY_en <= 0;
	 STOP_reg <= 1;
	 STOP_en <= 0;
	 DEVSEL_reg <= 1;
	 DEVSEL_en <= 0;
      end

   endtask // do_bar2_write

   // A simple state machine to request new transactions.
   always @(posedge CLK)
     if (REQ == 1 && FRAME_reg == 1 && IRDY_reg==1 && dma_count && dma_go)
       REQ <= 0;

   // This implements a DMA bus mastering read from an external
   // address to local memory.
   task do_master_read;	//write to memory while acting as PCI Master

      localparam read_cmd  = 4'hC;  // burst read command
      localparam DAC_cmd   = 4'hd;  // DAC
      reg [31:0] address_out;
      begin
         if (dma_dac64) begin
	    AD_reg <= dma_external_l; // low 32 bits of DAC address
	    AD_en  <= 1;

	    C_BE_reg <= DAC_cmd;      // reading from a PCI source
	    C_BE_en <= 1;

            FRAME_reg <= 0;	      // drive frame active
            FRAME_en  <= 1;

	    parity_bit = ^{dma_external_l, DAC_cmd};

	    @(posedge CLK) ;    // advance to address cycle
                                // ignore device select for now
	    address_out = dma_external_h;

	 end else begin
	    address_out = dma_external_l;
	 end

	 AD_reg <= address_out;    //source_adr is a register in BAR2 range
	 AD_en  <= 1;

	 C_BE_reg <= read_cmd;	    //reading from a PCI source
	 C_BE_en <= 1;

         FRAME_reg <= 0;	    //drive frame active
         FRAME_en  <= 1;

	 parity_bit = ^{address_out, read_cmd};

	 @(posedge CLK) ;    // advance to turnaround cycle
                             //ignore device select for now
	 FRAME_reg <= 0;     //leave frame active
	 FRAME_en  <= 1;
	 AD_en <= 0;	     //stop driving address

	 IRDY_reg <= 0;	     //turn on IRdy
	 IRDY_en  <= 1;

	 C_BE_reg <= 4'd0;   //read 4 bytes
	 C_BE_en <= 1;

         PAR_reg <= parity_bit;	 //parity of address
         PAR_en <= 1;

	 if ((IRDY == 0) && (TRDY == 0)) begin
	    write_val = AD;	//data to write to memory
	    // Store the next value
	    $simbus_mem_poke(memory_fd, dma_memory, write_val, 0);
	    // Linear addressing of memory
	    dma_memory <= dma_memory + 4;
	    dma_count <= dma_count - 4;
 	 end

	 // Keep reading so long as the target does not
	 // stop the transaction. Could add a burst count later.
	 while (STOP == 1)	
	   @(posedge CLK) begin //stop may be 0 by the time Clk rises
              PAR_en <= 0;	//stop driving parity
	      if (STOP==0)	
	         FRAME_reg <= 1;   //frame is inactive after stop
              if ((IRDY == 0) && (TRDY == 0)) begin
		 write_val = AD;
		 // Store the next value
		 $simbus_mem_poke(memory_fd, dma_memory, write_val, 0);
		 // Linear addressing.
		 dma_memory <= dma_memory + 4;
		 dma_count <= dma_count - 4;
	      end
            end

	 // Transaction done. Turn off drivers.
	 @(posedge CLK) begin
	    IRDY_reg <= 1;
	    FRAME_en <= 0;
	    IRDY_en <= 0;
	    C_BE_en <= 0;
            PAR_en <= 0;	
         end
      end

   endtask // do_master_read

   reg addr_dac32_flag = 0;

   reg frame_dly;
   wire addr_flag = (FRAME===1'b0) && (frame_dly===1'b1);
   always @(posedge CLK)
     if (RESET == 0)
       frame_dly <= 1'b1;
     else
       frame_dly <= FRAME;

   // This is the main loop that sits here waiting for a bus cycle
   // to start. Then, I kick in the state machines to complete the
   // target side of any transaction.
   always @(posedge CLK) begin

      if (addr_dac32_flag) begin
	 use_address[63:32] = AD;
	 addr_dac32_flag <= 0;
      end else if (addr_flag) begin
	 use_address[63:32] = 0;
	 use_address[31:0] = AD;
      end

      if (RESET == 0) begin
	 addr_dac32_flag <= 0;
	 do_reset;

      end else if ((FRAME == 1'b1) && (IRDY==1'b1) && (GNT == 0))
	begin
	   REQ <= 1;
	   do_master_read;
	end

      else if ((FRAME == 1'b1) && pending_flag)
	pending_flag = 0;

      else if (pending_flag)
	;

      else if ((FRAME == 1'b0) && (C_BE[3:0] == 4'b1010) && (IDSEL == 1'b1))
	do_configuration_read;

      else if ((FRAME == 1'b0) && (C_BE[3:0] == 4'b1011) && (IDSEL == 1'b1))
	do_configuration_write;

      else if ((FRAME == 1'b0) && (C_BE[3:0] == 4'b0110) && selected(use_address))
	do_memory_read(0);

      else if ((FRAME == 1'b0) && (C_BE[3:0] == 4'b1100) && selected(use_address))
	do_memory_read(0);

      else if ((FRAME == 1'b0) && (C_BE[3:0] == 4'b1110) && selected(use_address))
	do_memory_read(0);

      else if ((FRAME == 1'b0) && (C_BE[3:0] == 4'b0111) && selected(use_address))
	do_memory_write(0);

      else if ((FRAME == 1'b0) && (C_BE[3:0] == 4'b0110) && selected1(use_address))
	do_memory_read(1);

      else if ((FRAME == 1'b0) && (C_BE[3:0] == 4'b0111) && selected1(use_address))
	do_memory_write(1);

      else if ((FRAME == 1'b0) && (C_BE[3:0] == 4'b0110) && selected2(use_address))
	do_bar2_read;

      else if ((FRAME == 1'b0) && (C_BE[3:0] == 4'b0111) && selected2(use_address))
	do_bar2_write;

      else if ((FRAME == 1'b0) && (C_BE[3:0] == 4'b1101)) begin
	 addr_dac32_flag <= 1;

      end else if (FRAME == 1'b0)
	pending_flag = 1;

      else begin
      end

   end

   initial begin
      if (0 == $value$plusargs("bar0-mask=%h", bar0_mask))
	bar0_mask = 32'hffff_0000;
      // Constrain the range for the BAR0 mask.
      bar0_mask = 32'h8000_0000 | (bar0_mask & 32'hffff_f000);
      $display("%m: Using BAR0 mask = %8h", bar0_mask);

      // Open the memory image.
      memory_fd = $simbus_mem_open(IMAGE, 1 + ~bar0_mask);
      $display("%m: Open file %s as memory store. (fd=%0d)", IMAGE, memory_fd);

      // Get a retry rate from the command line. If there is none
      // specified, then use the value from the defined parameter.
      if (0 == $value$plusargs("mem-retry-rate=%d", retry_rate))
	retry_rate = 0;

      if (0 == $value$plusargs("mem-discon-rate=%d", discon_rate))
	discon_rate = 0;

   end

endmodule // pci_memory
