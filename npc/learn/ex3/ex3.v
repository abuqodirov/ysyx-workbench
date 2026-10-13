module ex3(
	input go,
	input clk,
	input rst,
	output busy
);

	localparam A = 0;
	localparam B = 1;
	reg current_state;
	
	reg [1:0] cycle_count;
	
	always @(posedge clk) begin
		if (rst) begin
			current_state <= A;
			cycle_count <= 2'b0;
		end else begin
			case (current_state)
				A: begin
					cycle_count <= 2'd0;
					if (go) begin
						current_state <= B;
					end
				end
				B: begin
					cycle_count <= cycle_count + 2'd1;
					if (cycle_count == 2'd3) begin
						current_state <= A;
					end
				end	
			endcase
	end
end

	assign busy  = (current_state == B);


endmodule
