module ex1(
	input clk,
	input rst,
	output reg [7:0] nb_a,
	output reg [7:0] nb_b,
	output reg [7:0] b_a,
	output reg [7:0] b_b
);

	always @(posedge clk) begin
		if (rst) begin
			nb_a <= 8'h11;
			nb_b <= 8'h22;
		end else begin
			nb_a <= nb_b;
			nb_b <= nb_a;
		end
	end

	always @(posedge clk) begin
		if (rst) begin
			b_a = 8'h11;
			b_b = 8'h22;
		end else begin
			b_a = b_b;
			b_b = b_a;
		end
	end

endmodule
