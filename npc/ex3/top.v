module top(
	input [3:0] A,
	input [3:0] B,
	input [2:0] sel,
	output reg [3:0] out,
	output reg carry,
	output reg overflow,
	output reg zero
);

	reg [4:0] add_sub_res;

	always @(*) begin
		carry = 1'b0;
		overflow = 1'b0;
		zero = 1'b0;
		out = 4'b0000;
		add_sub_res = 5'b0;

		case (sel) 
			3'b000: begin
				add_sub_res = {1'b0, A} + {1'b0, B};
				out = add_sub_res[3:0];
				carry = add_sub_res[4];
				overflow = (A[3] == B[3]) && (out[3] != A[3]);
			end

			3'b001: begin
				add_sub_res = {1'b0, A} - {1'b0, B};
				out = add_sub_res[3:0];
				carry = add_sub_res[4];
				overflow = (A[3] != B[3]) && (out[3] == B[3]);
			end

			3'b010: begin
				out = ~A;
			end

			3'b011: begin
				out = A & B;
			end

			3'b100: begin
				out = A | B;
			end

			3'b101: begin
				out = A ^ B;
			end

			3'b110: begin
				out = ($signed(A) < $signed(B)) ? 4'b0001 : 4'b0000;
			end

			3'b111: begin
				out = (A == B) ? 4'b0001 : 4'b0000;
			end

			default: out = 4'b0000;
		endcase

		if (out == 4'b0000) begin
			zero = 1'b1;
		end
	end

endmodule
