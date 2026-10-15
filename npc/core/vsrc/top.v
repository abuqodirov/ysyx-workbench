module top #(
	parameter [31:0] RESET_PC = 32'h3000_0000)
	(
        input clock,
        input reset,

        output [31:0] io_ifu_addr,
        input  [31:0] io_ifu_rdata,
        output        io_ifu_reqValid,
        input         io_ifu_respValid,

        output [31:0] io_lsu_addr,
        output [31:0] io_lsu_wdata,
        output [3:0]  io_lsu_wmask,
        output        io_lsu_wen,
        output        io_lsu_reqValid,
        input         io_lsu_respValid,
        input  [31:0] io_lsu_rdata,
				output [1:0]  io_lsu_size,

        output reg [31:0] pc,
        output inst_retired,
        output [6:0] funct7_out
);
		
        localparam STATE_FETCH = 2'b00;
        localparam STATE_EXEC  = 2'b01;
        localparam STATE_MEM   = 2'b10;
        reg [1:0] current_state;

        reg [31:0] inst_reg;
        always @(posedge clock) begin
                if (current_state == STATE_FETCH && io_ifu_respValid) begin
                        inst_reg <= io_ifu_rdata;
                end
        end

        wire [31:0] inst = inst_reg;

        assign io_ifu_addr = pc;
        assign io_ifu_reqValid = !reset && (current_state == STATE_FETCH);

        assign io_lsu_reqValid = (current_state == STATE_MEM);
        wire [6:0] opcode = inst[6:0];
        wire [4:0] rd = inst[11:7];
        wire [4:0] rs1 = inst[19:15];
        wire [4:0] rs2 = inst[24:20];
        wire [2:0] funct3 = inst[14:12];
        wire [6:0] funct7 = inst[31:25];
        assign funct7_out = funct7;

        wire [31:0] imm_i = {{20{inst[31]}}, inst[31:20]};
        wire [31:0] imm_s = {{20{inst[31]}}, inst[31:25], inst[11:7]};
        wire [31:0] imm_u = {inst[31:12], 12'b0};

        wire is_rtype = (opcode == 7'b0110011);
        wire is_addi  = (opcode == 7'b0010011);
        wire is_jalr  = (opcode == 7'b1100111);
        wire is_lui   = (opcode == 7'b0110111);
        wire is_load  = (opcode == 7'b0000011);
        wire is_store = (opcode == 7'b0100011);
        wire is_sub   = is_rtype & (funct7[5] == 1'b1);
        wire is_mem_op = is_store | is_load;

				reg [63:0] cycle_cnt; 

        assign io_lsu_wen = is_store & io_lsu_reqValid;
				localparam [11:0] CSR_MVENDORID = 12'hF11;
				localparam [11:0] CSR_MARCHID = 12'hF12;
				localparam [31:0] MVENDORID_VAL = 32'h79737978;
				localparam [31:0] MARCHID_VAL = 32'h00414255;
				localparam [11:0] CSR_MCYCLE = 12'hb00;
				localparam [11:0] CSR_MCYCLEH = 12'hB80;

				wire is_system = (opcode == 7'b1110011);
				wire is_csrrs = is_system & (funct3 == 3'b010);
				wire [11:0] csr_addr = inst[31:20];
				wire [31:0] csr_rdata = (csr_addr == CSR_MVENDORID) ? MVENDORID_VAL : (csr_addr == CSR_MARCHID) ? MARCHID_VAL : (csr_addr == CSR_MCYCLE) ? cycle_cnt[31:0] : (csr_addr == CSR_MCYCLEH) ? cycle_cnt[63:32] : 32'b0;

        wire wen = is_addi | is_jalr | is_rtype | is_lui | is_load | is_csrrs;

        wire [31:0] rs1_data;
        wire [31:0] rs2_data;
        wire [31:0] alu_operand_a = rs1_data;
        wire [31:0] alu_operand_b = is_rtype ? rs2_data : is_store ? imm_s : imm_i;
        wire [31:0] alu_out = is_sub ? (alu_operand_a - alu_operand_b) : (alu_operand_a + alu_operand_b);

        assign io_lsu_addr = alu_out;

        wire is_sw = (funct3 == 3'b010);
        wire [31:0] store_byte_data = {4{rs2_data[7:0]}};
        wire [3:0] store_byte_mask = 4'b0001 << alu_out[1:0];
        assign io_lsu_wdata = is_sw ? rs2_data : store_byte_data;
        assign io_lsu_wmask = is_sw ? 4'b1111 : store_byte_mask;

        wire [7:0] extracted_byte = (alu_out[1:0] == 2'b00) ? io_lsu_rdata[7:0] : (alu_out[1:0] == 2'b01) ? io_lsu_rdata[15:8] : (alu_out[1:0] == 2'b10) ? io_lsu_rdata[23:16] : io_lsu_rdata[31:24];
        wire is_lw = (funct3 == 3'b010);
        wire [31:0] load_data = is_lw ? io_lsu_rdata : {24'b0, extracted_byte};

        wire [31:0] wdata = is_jalr ? (pc + 32'd4) : is_lui ? imm_u : is_load ? load_data: is_csrrs ? csr_rdata : alu_out;

        assign inst_retired = (current_state == STATE_EXEC && !is_mem_op) || 
                              (current_state == STATE_MEM && io_lsu_respValid);

        wire actual_rf_wen = wen & inst_retired;
				assign io_lsu_size = funct3[1:0];


				`ifndef SYNTHESIS
					wire is_ebreak = (inst == 32'h00100073);
					wire [31:0] a0_value  = rf_inst.rf[10];
				`ifdef OSOC_KIT
        // OSOC user kit: report retired instructions and ebreak
        // through the kit's own NPCTrap module (DPI set_commit).
					reg        kit_valid, kit_ebreak;
					reg [31:0] kit_code;
					always @(posedge clock) begin
                kit_valid  <= !reset && inst_retired;
                kit_ebreak <= is_ebreak;
                kit_code   <= a0_value;
					end
					NPCTrap npctrap (
                .valid    (kit_valid),
                .isEbreak (kit_ebreak),
                .code     (kit_code),
                .isMMIO   (1'b0)
					);
				`elsif __ICARUS__
					always @(posedge clock) begin
						if (!reset && is_ebreak && inst_retired) begin
							if (a0_value == 32'b0) $display("\nHIT GOOD TRAP (after %0d cycles)", $time / 2);
                        else                   $display("\nHIT BAD TRAP (a0 = %0d)", a0_value);
                        $finish;
						end
					end
				`else
					import "DPI-C" context function void halt_sim(input int a0_value);
					import "DPI-C" function void notify_retire();

					always @(posedge clock) begin
                if (!reset && is_ebreak && inst_retired) halt_sim(a0_value);
					end

					always @(posedge clock) begin
                if (!reset && inst_retired) notify_retire();
					end
				`endif
				`endif

				always @(posedge clock) begin
					if (reset) cycle_cnt <= 64'b0;
					else cycle_cnt <= cycle_cnt + 64'd1;
				end

        RegisterFile #(5, 32) rf_inst (
                .clk(clock),
                .wdata(wdata),
                .waddr(rd),
                .wen(actual_rf_wen),
                .raddr1(rs1),
                .rdata1(rs1_data),
                .raddr2(rs2),
                .rdata2(rs2_data)
        );

        always @(posedge clock) begin
                if (reset) begin
                        pc <= RESET_PC;
                        current_state <= STATE_FETCH; 
                end
                else begin
                        if (inst_retired) begin
                                if (is_jalr) pc <= alu_out & 32'hFFFFFFFE;
                                else         pc <= pc + 32'd4;
                        end
                        
                        case (current_state) 
                                STATE_FETCH: begin
                                        if (io_ifu_respValid) current_state <= STATE_EXEC;
                                end
                                STATE_EXEC: begin
                                        if (is_mem_op) current_state <= STATE_MEM;
                                        else           current_state <= STATE_FETCH;
                                end
                                STATE_MEM: begin
                                        if (io_lsu_respValid) current_state <= STATE_FETCH;
                                end
                                default: current_state <= STATE_FETCH;
                        endcase
                end
        end
endmodule
