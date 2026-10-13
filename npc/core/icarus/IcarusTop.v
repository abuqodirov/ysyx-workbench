// Icarus Verilog testbench for E8's four-state simulation. Wraps the same
// NpcSim (CPU + SimpleBus memory) as the Verilator standalone build; memory
// goes through VPI ($pmem_*) instead of DPI-C.
//   vvp -M core/icarus -mpmem_vpi icarus.vvp +IMG=<image.bin> [+MAX_CYCLES=n] [+WAVE]
module IcarusTop;
  reg clock = 1'b0;
  reg reset = 1'b1;
  always #1 clock = ~clock;                 // one cycle = 2 time units

  NpcSim sim (.clock(clock), .reset(reset));

  integer cycle = 0;
  integer max_cycles;
  reg     dumping = 1'b0;

  initial begin
    if (!$value$plusargs("MAX_CYCLES=%d", max_cycles)) max_cycles = 100000000;
    if ($test$plusargs("WAVE")) begin       // waveform of the first 5000 cycles
      $dumpfile("icarus.vcd");
      $dumpvars(0, IcarusTop);
      dumping = 1'b1;
    end
    $pmem_load;                             // reads +IMG=<path> itself
    repeat (10) @(posedge clock);
    reset <= 1'b0;                          // release just after an edge
  end

  always @(posedge clock) begin
    cycle <= cycle + 1;
		if (cycle != 0 && cycle % 1000000 == 0) $display("[icarus] %0d M cycles", cycle / 1000000);
    if (dumping && cycle == 5000) $dumpoff;
    if (cycle == max_cycles) begin
      $display("\n[icarus] no ebreak after %0d cycles -- stopping", cycle);
      $finish;
    end
  end
endmodule
