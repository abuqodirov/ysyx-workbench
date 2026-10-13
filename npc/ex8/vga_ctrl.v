module vga_ctrl(
    input           pclk,     // 25MHz pixel clock
    input           reset,    
    input  [23:0]   vga_data, // RGB888 color input
    output [9:0]    h_addr,   // Current X coordinate (0-639)
    output [9:0]    v_addr,   // Current Y coordinate (0-479)
    output          hsync,    
    output          vsync,    
    output          valid,    
    output [7:0]    vga_r,    
    output [7:0]    vga_g,    
    output [7:0]    vga_b    
);
  parameter    h_frontporch = 96;
  parameter    h_active = 144;
  parameter    h_backporch = 784;
  parameter    h_total = 800;
  parameter    v_frontporch = 2;
  parameter    v_active = 35;
  parameter    v_backporch = 515;
  parameter    v_total = 525;

  reg [9:0]    x_cnt;
  reg [9:0]    y_cnt;
  wire         h_valid;
  wire         v_valid;

  // Changed from async (posedge reset) to purely synchronous (posedge pclk)
  always @(posedge pclk) begin
      if (reset == 1'b1)
        x_cnt <= 1;
      else begin
        if (x_cnt == h_total)
            x_cnt <= 1;
        else
            x_cnt <= x_cnt + 10'd1;
      end
  end

  always @(posedge pclk) begin
      if (reset == 1'b1)
        y_cnt <= 1;
      else begin
        if (y_cnt == v_total && x_cnt == h_total)
            y_cnt <= 1;
        else if (x_cnt == h_total)
            y_cnt <= y_cnt + 10'd1;
      end
  end

  assign hsync = (x_cnt > h_frontporch);
  assign vsync = (y_cnt > v_frontporch);

  assign h_valid = (x_cnt > h_active) && (x_cnt <= h_backporch);
  assign v_valid = (y_cnt > v_active) && (y_cnt <= v_backporch);
  assign valid = h_valid & v_valid;

  assign h_addr = h_valid ? (x_cnt - 10'd145) : 10'd0;
  assign v_addr = v_valid ? (y_cnt - 10'd36) : 10'd0;

  assign vga_r = valid ? vga_data[23:16] : 8'h00;
  assign vga_g = valid ? vga_data[15:8]  : 8'h00;
  assign vga_b = valid ? vga_data[7:0]   : 8'h00;

endmodule
