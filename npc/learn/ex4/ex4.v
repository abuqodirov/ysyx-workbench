module ex4 #(
parameter LATENCY = 5)
(
	input clk,
	input rst,
	input reqValid,
	input [31:0] addr,
	output reg respValid,
	output reg [31:0] rdata
);

	reg [31:0] mem [0:255];

	integer i;
	initial begin
		for (i=0; i < 256; i = i+1) 
			mem[i] = 32'hA0000000 + i;
	end
  
	reg [31:0] count;

	localparam IDLE = 1'b0;
	localparam WAIT = 1'b1;
	reg current;


	always @(posedge clk) begin 
		if (rst) begin
			current <= IDLE;
			count <= 32'd0;
			respValid <= 1'b0;
		end else begin
			respValid <= 1'b0;
			case (current) 
				IDLE: begin
					if (reqValid) begin
						if (LATENCY == 0) begin
							rdata <= mem[addr[9:2]];
							respValid <= 1'b1;
							current <= IDLE;
						end else begin
							count <= LATENCY;
							current <= WAIT;
						end
					end
				end
				WAIT: begin
					count <= count - 32'd1;
					if (count == 32'd1) begin
						rdata <= mem[addr[9:2]];
						respValid <= 1'b1;
						current <= IDLE;
					end
				end
			endcase
		end
	end


endmodule
