module top(
  input clk,
  input rst,
  output reg [7:0] lfsr_out,
  output reg [6:0] hex0,
  output reg [6:0] hex1
);

  // Feedback bit based on the prompt's formula: x4 ^ x3 ^ x2 ^ x0
  wire feedback = lfsr_out[4] ^ lfsr_out[3] ^ lfsr_out[2] ^ lfsr_out[0];

  // Sequential logic for the LFSR shift register
  always @(posedge clk) begin
    if (rst) begin
      lfsr_out <= 8'b00000001; // Seed value (must not be all zeros!)
    end else begin
      lfsr_out <= {feedback, lfsr_out[7:1]}; // Non-blocking assignment
    end
  end

  // 7-segment decoder function placed properly inside the module scope
  function [6:0] dec_7seg(input [3:0] val);
    case (val)
      4'h0: dec_7seg = ~7'b1111110;
      4'h1: dec_7seg = ~7'b0110000;
      4'h2: dec_7seg = ~7'b1101101;
      4'h3: dec_7seg = ~7'b1111001;
      4'h4: dec_7seg = ~7'b0110011;
      4'h5: dec_7seg = ~7'b1011011;
      4'h6: dec_7seg = ~7'b1011111;
      4'h7: dec_7seg = ~7'b1110000;
      4'h8: dec_7seg = ~7'b1111111;
      4'h9: dec_7seg = ~7'b1111011;
      4'hA: dec_7seg = ~7'b1110111;
      4'hB: dec_7seg = ~7'b0011111;
      4'hC: dec_7seg = ~7'b1001110;
      4'hD: dec_7seg = ~7'b0111101;
      4'hE: dec_7seg = ~7'b1001111;
      4'hF: dec_7seg = ~7'b1000111;
      default: dec_7seg = ~7'b0000000;
    endcase
  endfunction

  // Combinational logic to map the upper and lower nibbles to the hex displays
  always @(*) begin
    hex0 = dec_7seg(lfsr_out[3:0]);
    hex1 = dec_7seg(lfsr_out[7:4]);
  end

endmodule
