module top(
  input clk,
  input rst,
  output [7:0] vga_r,
  output [7:0] vga_g,
  output [7:0] vga_b,
  output vga_hs,
  output vga_vs,
  output vga_blank_n
);

  wire [9:0] h_addr;
  wire [9:0] v_addr;
  wire valid;

  // Full VRAM Array (640 * 480 = 307,200 words)
  reg [23:0] vram [0:307199];

  initial begin
    $readmemh("ex8/picture_raw.mif", vram);
  end

  wire [23:0] pixel_color;

  // Pass main `clk` directly to `pclk` to match NVBoard's update rate
  vga_ctrl controller(
    .pclk(clk), 
    .reset(rst),
    .vga_data(pixel_color),
    .h_addr(h_addr),
    .v_addr(v_addr),
    .hsync(vga_hs),
    .vsync(vga_vs),
    .valid(valid),
    .vga_r(vga_r),
    .vga_g(vga_g),
    .vga_b(vga_b)
  );

  assign vga_blank_n = valid;

  // Linear 1D Memory Address: (v_addr * 640) + h_addr (19 bits wide)
  wire [18:0] mem_addr = ({9'b0, v_addr} * 19'd640) + {9'b0, h_addr};

  // Output RGB pixel data during active frame
  assign pixel_color = valid ? vram[mem_addr] : 24'h000000;

endmodule
