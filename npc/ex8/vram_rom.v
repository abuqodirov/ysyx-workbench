module vram_rom (
  input wire [11:0] address,
  input wire clock,
  output reg [23:0] q
);

  // Quartus attribute to bind the MIF file to FPGA block RAM
  (* ram_init_file = "picture.mif" *) reg [23:0] memory [0:4095];

  initial begin
    $readmemh("picture.hex", memory); // Fallback for Verilator linting
  end

  always @(posedge clock) begin
    q <= memory[address];
  end

endmodule
