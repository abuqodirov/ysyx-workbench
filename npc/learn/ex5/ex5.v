module ex5(
	input [31:0] inst,
  output [4:0] rs1,
	output [4:0] rs2,
	output [4:0] rd,
	output [31:0] imm,
	output [8:0] onehot
);

  wire [6:0] opcode = inst[6:0];
	wire [4:0] inst_rd = inst[11:7];
	wire [4:0] inst_rs1 = inst[19:15];
	wire [4:0] inst_rs2 = inst[24:20];
	wire [2:0] funct3 = inst[14:12];
	wire [6:0] funct7 = inst[31:25];

	wire is_rtype = (opcode == 7'b0110011);
	wire is_addi  = (opcode == 7'b0010011) && (funct3 == 3'b000);
	wire is_jalr  = (opcode == 7'b1100111);
	wire is_lui   = (opcode == 7'b0110111);
	wire is_load  = (opcode == 7'b0000011);
	wire is_store = (opcode == 7'b0100011);

	wire is_add = (opcode == 7'b0110011) && (funct3 == 3'b000) && (funct7 == 7'b0000000);
	wire is_sub = (opcode == 7'b0110011) && (funct3 == 3'b000) && (funct7 == 7'b0100000);
	wire is_sw = is_store && (funct3 == 3'b010);
	wire is_sb = is_store && (funct3 == 3'b000);
	wire is_lw = is_load && (funct3 == 3'b010);
	wire is_lbu = is_load && (funct3 == 3'b100);
	
	wire [31:0] imm_i = { {20{inst[31]}}, inst[31:20]};
	wire [31:0] imm_s = { {20{inst[31]}}, inst[31:25], inst[11:7]};
	wire [31:0] imm_u = {inst[31:12], 12'b0};

	assign onehot = {is_sb, is_sw, is_lbu, is_lw, is_jalr, is_sub, is_add, is_addi, is_lui};

	assign rs1 = inst_rs1;
	assign rs2 = inst_rs2;
	assign rd = inst_rd;

	assign imm = is_lui                                   ? imm_u :
             (is_addi | is_jalr | is_lw | is_lbu)     ? imm_i :
             (is_sw   | is_sb)                        ? imm_s :
                                                        32'b0;

endmodule
