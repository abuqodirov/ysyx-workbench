module top(
  input clk,
  input rst,
  output reg [15:0] led,
  output [7:0] pc_out // <-- Exposes PC to C++ for the difftest
);

  // 1. Program Counter
  reg [7:0] pc;
  assign pc_out = pc;

  // 2. Instruction Memory (ROM)
  reg [15:0] rom [0:255];

  initial begin
    rom[0] = 16'h0000; // LI  r0, 0   (sum = 0)
    rom[1] = 16'h1001; // LI  r1, 1   (current_num = 1)
    rom[2] = 16'h2001; // LI  r2, 1   (step = 1)
    rom[3] = 16'h300B; // LI  r3, 11  (limit = 11)
    rom[4] = 16'h4100; // ADD r0, r0, r1
    rom[5] = 16'hC000; // OUT r0         (Updates LED inside the loop)
    rom[6] = 16'h5600; // ADD r1, r1, r2
    rom[7] = 16'h9FFD; // BNE r1, r3, -3 (Jump back to PC=4)
    rom[8] = 16'h8400; // BNE r0, r1, 0  (Halt loop)
  end

  wire [15:0] inst = rom[pc];

  // 3. Instruction Field Decoding
  wire [1:0] opcode = inst[15:14];
  wire [1:0] rd     = inst[13:12];

  // Dynamic Muxing for rs1 and rs2 based on opcode format
  wire [1:0] rs1 = (opcode == 2'b01) ? inst[11:10] : inst[13:12];
  wire [1:0] rs2 = (opcode == 2'b01) ? inst[9:8]   : inst[11:10];

  wire [11:0] imm12 = inst[11:0];
  wire [7:0] offset = inst[7:0];

  // 4. Register File
  reg [15:0] regs [0:3];

  wire [15:0] reg_rs1 = regs[rs1];
  wire [15:0] reg_rs2 = regs[rs2];

  // 5. Execution Logic
  wire [15:0] alu_add = reg_rs1 + reg_rs2;
  wire branch_taken = (opcode == 2'b10) && (reg_rs1 != reg_rs2);

  // 6. PC Update Logic
  always @(posedge clk) begin
    if (rst) begin
      pc <= 8'd0;
    end else if (branch_taken) begin
      pc <= pc + offset;
    end else begin
      pc <= pc + 8'd1;
    end
  end

  // 7. Register Writeback & LED IO
  always @(posedge clk) begin
    if (rst) begin
      regs[0] <= 16'd0;
      regs[1] <= 16'd0;
      regs[2] <= 16'd0;
      regs[3] <= 16'd0;
      led     <= 16'd0;
    end else begin
      case (opcode)
        2'b00: regs[rd] <= {4'b0, imm12};
        2'b01: regs[rd] <= alu_add;
        2'b11: led      <= reg_rs1;
        default: ;
      endcase
    end
  end

endmodule
