module RegisterFile #(parameter ADDR_WIDTH=5, parameter DATA_WIDTH=32) (
	input clk,
	input [DATA_WIDTH-1:0] wdata,
	input [ADDR_WIDTH-1:0] waddr,
	input wen,
	input [ADDR_WIDTH-1:0] raddr1,
	input [ADDR_WIDTH-1:0] raddr2,
	output [DATA_WIDTH-1:0] rdata1,
	output [DATA_WIDTH-1:0] rdata2
);

	reg [DATA_WIDTH-1:0] rf [2**ADDR_WIDTH-1:0];

`ifndef SYNTHESIS
`ifndef __ICARUS__
	integer i;
	initial begin
		for (i=0; i<32; i=i+1) rf[i] = 0;
	end
`endif
`endif

	always @(posedge clk) begin
		if (wen && (waddr != 0)) begin
			rf[waddr] <= wdata;
		end
	end

	assign rdata1 = (raddr1 == 0) ? 32'b0 : rf[raddr1];
	assign rdata2 = (raddr2 == 0) ? 32'b0 : rf[raddr2];

endmodule
