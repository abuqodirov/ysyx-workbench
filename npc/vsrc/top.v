module top(
  input clk,
  input rst,
  output reg [15:0] led
);
  reg [31:0] count;
  
  always @(posedge clk) begin
    if (rst) begin 
      led <= 16'b1; // Initialize the first LED
      count <= 0; 
    end
    else begin
      // Shift the LED left by one, moving the highest bit to the lowest
      if (count == 0) led <= {led[14:0], led[15]};
      
      // Delay counter
      count <= (count >= 5000000 ? 32'b0 : count + 1);
    end
  end
endmodule
