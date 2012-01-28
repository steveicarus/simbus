
module main;

   wire [7:0] adata;
   wire       aclk;
   reg 	      aresp = 1'b0;
   point_slave_io #(.name("a1"), .WIDTH_O(8), .WIDTH_I(1)) a1
     (.clock(aclk),
      .data_o(adata),
      .data_i(aresp)
      /* */);

   reg [7:0]  bdata;
   wire       bclk, bresp;
   point_master_io #(.name("b1"), .WIDTH_O(8), .WIDTH_I(1)) b1
     (.clock(bclk),
      .data_o(bdata),
      .data_i(bresp)
      /* */);

   reg [7:0]  adata_del;
   always @(posedge aclk)
      adata_del <= adata;

   always @(posedge bclk)
     bdata <= adata_del;

endmodule // main
