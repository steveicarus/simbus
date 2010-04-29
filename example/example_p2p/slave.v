
module main;

   wire clock;
   wire [15:0] data_i, data_o;

   point_slave_io #(.name("slave"), .WIDTH_O(16), .WIDTH_I(16)) slot
     (.clock(clock),
      .data_o(data_o),
      .data_i(data_i)
      /* */);

   dut U1
     (.clock(clock),
      .data_o(data_o),
      .data_i(data_i)
      /* */);

endmodule // main

module dut
  (input wire clock,
   input wire[15:0]data_o,
   output reg[15:0]data_i
   /* */);

   always @(posedge clock)
     data_i <= ~data_o;

endmodule // dut
