module ex6(
	input clk,
	input [4:0] waddr,
	input [31:0] wdata,
	input wen,
	input [4:0] raddr1,
	input [4:0] raddr2,
	output [31:0] rdata1,
	output [31:0] rdata2
);


	reg [31:0] rf [0:31];

	always @(posedge clk) begin
		if (wen) begin
			rf[waddr] <= wdata;
		end
	end

	assign rdata1 = (raddr1 == 0) ? 32'b0 : rf[raddr1];
	assign rdata2 = (raddr2 == 0) ? 32'b0 : rf[raddr2];

endmodule
