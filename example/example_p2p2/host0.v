
module main;

   reg [7:0] adata;
   wire      aclk, aresp;
   point_master_io #(.name("a0"), .WIDTH_O(8), .WIDTH_I(1)) a0
     (.clock(aclk),
      .data_o(adata),
      .data_i(aresp)
      /* */);

   wire [7:0] bdata;
   wire       bclk;
   reg 	      bresp = 1'b1;
   point_slave_io #(.name("b0"), .WIDTH_O(8), .WIDTH_I(1)) b0
     (.clock(bclk),
      .data_o(bdata),
      .data_i(bresp)
      /* */);

   integer    idx;
   initial begin
      adata <= 0;
      for (idx = 0 ; idx < 255 ; idx = idx+1) begin
	 adata <= idx;
	 @(posedge aclk) ;
	 @(posedge aclk) ;
      end
      $display("%m: done");
      $finish;
   end

   always @(posedge bclk) begin
      $display("bdata = %h", bdata);
   end

endmodule // main
