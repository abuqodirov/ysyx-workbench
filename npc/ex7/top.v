module top(
  input clk,
  input clrn,
  input ps2_clk,
  input ps2_data,
  output overflow,       
  output reg [6:0] hex0,
  output reg [6:0] hex1,
  output reg [6:0] hex2,
  output reg [6:0] hex3,
  output reg [6:0] hex4,
  output reg [6:0] hex5
);

  wire [7:0] data;
  wire ready;
  reg nextdata_n;

  ps2_keyboard my_keyboard(
    .clk(clk),
    .clrn(clrn),
    .ps2_clk(ps2_clk),
    .ps2_data(ps2_data),
    .data(data),
    .ready(ready),
    .nextdata_n(nextdata_n),
    .overflow(overflow)  
  );

  reg [7:0] current_scan_code;
  reg [7:0] ascii_code;
  reg [15:0] press_count;
  reg receiving_break;

  always @(posedge clk) begin
    if (!clrn) begin
      current_scan_code <= 8'b0;
      press_count <= 16'b0;
      receiving_break <= 1'b0;
      nextdata_n <= 1'b1;
    end else begin
      nextdata_n <= 1'b1;
      if (ready && nextdata_n) begin
        nextdata_n <= 1'b0;
        
        if (data == 8'hF0) begin
          receiving_break <= 1'b1;
        end else begin
          if (receiving_break) begin
            press_count <= press_count + 1'b1;
            receiving_break <= 1'b0;
            current_scan_code <= 8'b0;
          end else begin
            current_scan_code <= data;
          end
        end
      end
    end
  end

  always @(*) begin
    case (current_scan_code)
      8'h1C: ascii_code = 8'h41; // 'A'
      8'h32: ascii_code = 8'h42; // 'B'
      8'h21: ascii_code = 8'h43; // 'C'
      8'h23: ascii_code = 8'h44; // 'D'
      8'h24: ascii_code = 8'h45; // 'E'
      8'h2B: ascii_code = 8'h46; // 'F'
      8'h34: ascii_code = 8'h47; // 'G'
      8'h33: ascii_code = 8'h48; // 'H'
      8'h43: ascii_code = 8'h49; // 'I'
      8'h3B: ascii_code = 8'h4A; // 'J'
      8'h42: ascii_code = 8'h4B; // 'K'
      8'h4B: ascii_code = 8'h4C; // 'L'
      8'h3A: ascii_code = 8'h4D; // 'M'
      8'h31: ascii_code = 8'h4E; // 'N'
      8'h44: ascii_code = 8'h4F; // 'O'
      8'h4D: ascii_code = 8'h50; // 'P'
      8'h15: ascii_code = 8'h51; // 'Q'
      8'h2D: ascii_code = 8'h52; // 'R'
      8'h1B: ascii_code = 8'h53; // 'S'
      8'h2C: ascii_code = 8'h54; // 'T'
      8'h3C: ascii_code = 8'h55; // 'U'
      8'h2A: ascii_code = 8'h56; // 'V'
      8'h1D: ascii_code = 8'h57; // 'W'
      8'h22: ascii_code = 8'h58; // 'X'
      8'h35: ascii_code = 8'h59; // 'Y'
      8'h1A: ascii_code = 8'h5A; // 'Z'
      default: ascii_code = 8'h00;
    endcase
  end

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

  always @(*) begin
    hex0 = dec_7seg(current_scan_code[3:0]);
    hex1 = dec_7seg(current_scan_code[7:4]);
    hex2 = dec_7seg(ascii_code[3:0]);
    hex3 = dec_7seg(ascii_code[7:4]);
    hex4 = dec_7seg(press_count[3:0]);
    hex5 = dec_7seg(press_count[7:4]);
  end

endmodule
