module top(
	input clk,
	input rst,
	input [31:0] inst,
	output reg [31:0] pc,

	output [31:0] dmem_addr,
	output [31:0] dmem_wdata,
	output [3:0]  dmem_wmask,
	output dmem_we,
	output dmem_re,
	input [31:0] dmem_rdata,
	output [6:0] funct7_out
);

  import "DPI-C" context function void halt_sim(input int a0_value);

	wire [6:0] opcode = inst[6:0];
	wire [4:0] rd = inst[11:7];
	wire [4:0] rs1 = inst[19:15];
	wire [4:0] rs2 = inst[24:20];
	wire [2:0] funct3 = inst[14:12];
	wire [6:0] funct7 = inst[31:25];

	wire [31:0] a0_value;
	
	wire [31:0] imm_i = {{20{inst[31]}}, inst[31:20]};
  wire [31:0] imm_s = {{20{inst[31]}}, inst[31:25], inst[11:7]};
  wire [31:0] imm_u = {inst[31:12], 12'b0};

	wire is_rtype = (opcode == 7'b0110011); // add
  wire is_addi  = (opcode == 7'b0010011);
  wire is_jalr  = (opcode == 7'b1100111);
  wire is_lui   = (opcode == 7'b0110111);
  wire is_load  = (opcode == 7'b0000011); // lw, lbu
  wire is_store = (opcode == 7'b0100011);
	wire is_sub = is_rtype & (funct7[5] == 1'b1);

	wire [31:0] rs1_data;
	wire [31:0] rs2_data;
	wire [31:0] wdata;
  wire wen = is_addi | is_jalr | is_rtype | is_lui | is_load;

	wire [31:0] alu_operand_a = rs1_data;
	wire [31:0] alu_operand_b = is_rtype ? rs2_data : is_store ? imm_s : imm_i;

	wire [31:0] alu_out = is_sub ? (alu_operand_a - alu_operand_b) : (alu_operand_a + alu_operand_b);

	assign dmem_addr = {alu_out[31:2], 2'b00};
	assign dmem_we = is_store;
	assign dmem_re = is_load;

	wire is_sw = (funct3 == 3'b010);
	wire [31:0] store_byte_data = {4{rs2_data[7:0]}};
	wire [3:0] store_byte_mask = 4'b0001 << alu_out[1:0];

	assign dmem_wdata = is_sw ? rs2_data : store_byte_data;
	assign dmem_wmask = is_sw ? 4'b1111 : store_byte_mask;

	wire [7:0] extracted_byte = (alu_out[1:0] == 2'b00) ? dmem_rdata[7:0] : (alu_out[1:0] == 2'b01) ? dmem_rdata[15:8] : (alu_out[1:0] == 2'b10) ? dmem_rdata[23:16] : dmem_rdata[31:24];
	wire is_lw = (funct3 == 3'b010);
	wire [31:0] load_data = is_lw ? dmem_rdata : {24'b0, extracted_byte};

	assign wdata = is_jalr ? (pc + 32'd4) : is_lui ? imm_u : is_load ? load_data: alu_out;

	wire is_ebreak = (inst == 32'h00100073);

	always @(posedge clk) begin
		if (is_ebreak) begin
			halt_sim(a0_value);
		end
	end

	RegisterFile #(5, 32) rf_inst (
        .clk(clk),
        .wdata(wdata),
        .waddr(rd),
        .wen(wen),
        .raddr1(rs1),
        .rdata1(rs1_data),
        .raddr2(rs2),
				.rdata2(rs2_data),
				.a0_val(a0_value)
  );

	always @(posedge clk) begin
		if (rst) begin
			pc <=32'h80000000;
		end
		else begin
			if (is_jalr) begin
				pc <= alu_out & 32'hFFFFFFFE;
			end
			else begin
				pc <= pc + 32'd4;
			end
		end
	end

	assign funct7_out = funct7;

endmodule
