module top(
	input [7:0] in,
	input en,
	output reg [2:0] pos,
	output valid,
	output [6:0] hex
);

	always @(*) begin
		pos = 3'b000;
		valid = 1'b0;

		if (en) begin
      // Using an if-else chain completely eliminates overlaps and priority ambiguity
      if      (in[7]) begin pos = 3'b111; valid = 1'b1; end
      else if (in[6]) begin pos = 3'b110; valid = 1'b1; end
      else if (in[5]) begin pos = 3'b101; valid = 1'b1; end
      else if (in[4]) begin pos = 3'b100; valid = 1'b1; end
      else if (in[3]) begin pos = 3'b011; valid = 1'b1; end
      else if (in[2]) begin pos = 3'b010; valid = 1'b1; end
      else if (in[1]) begin pos = 3'b001; valid = 1'b1; end
      else if (in[0]) begin pos = 3'b000; valid = 1'b1; end
      else            begin pos = 3'b000; valid = 1'b0; end
    end

		case (pos)
			3'b000: hex = 7'b1000000;
			3'b001: hex = 7'b1111001;
			3'b010: hex = 7'b0100100;
			3'b011: hex = 7'b0110000;
			3'b100: hex = 7'b0011001;
			3'b101: hex = 7'b0010010;
			3'b110: hex = 7'b0000010;
			3'b111: hex = 7'b1111000;
			default: hex = 7'b1111111;
		endcase
	end
endmodule
