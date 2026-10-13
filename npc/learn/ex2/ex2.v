module ex2(
	input clk,
	input rst, 
	input en,
	output reg [7:0] count
);

	always @(posedge clk) begin
		if (rst) begin
			count<=0;
		end	else if (en) begin
			count <= count + 8'b00000100;
		end
	end

endmodule
