module NpcSim (
  input clock,
  input reset
);
  wire [31:0] ifu_addr, ifu_rdata;
  wire        ifu_reqValid, ifu_respValid;
  wire [31:0] lsu_addr, lsu_wdata, lsu_rdata;
  wire [3:0]  lsu_wmask;
  wire        lsu_wen, lsu_reqValid, lsu_respValid;
  wire        inst_retired;

`ifdef NETLIST
  // The netlist has RESET_PC baked in (0x3000_0000); the memory model serves
  // a two-instruction boot stub there that jumps to 0x8000_0000.
  top cpu (
`else
  top #(.RESET_PC(32'h8000_0000)) cpu (
`endif
    .clock(clock), .reset(reset),
    .io_ifu_addr(ifu_addr),           .io_ifu_rdata(ifu_rdata),
    .io_ifu_reqValid(ifu_reqValid),   .io_ifu_respValid(ifu_respValid),
    .io_lsu_addr(lsu_addr),           .io_lsu_wdata(lsu_wdata),   .io_lsu_wmask(lsu_wmask),
    .io_lsu_wen(lsu_wen),             .io_lsu_reqValid(lsu_reqValid),
    .io_lsu_respValid(lsu_respValid), .io_lsu_rdata(lsu_rdata),
    .io_lsu_size(), .pc(), .inst_retired(inst_retired), .funct7_out()
  );

  // IFU and LSU each get their own port; both reach the same memory array.
  SimpleBusMem ifu_mem (.clock(clock), .reset(reset),
    .reqValid(ifu_reqValid), .addr(ifu_addr), .wen(1'b0), .wdata(32'b0), .wmask(4'b0),
    .respValid(ifu_respValid), .rdata(ifu_rdata));

  SimpleBusMem lsu_mem (.clock(clock), .reset(reset),
    .reqValid(lsu_reqValid), .addr(lsu_addr), .wen(lsu_wen), .wdata(lsu_wdata), .wmask(lsu_wmask),
    .respValid(lsu_respValid), .rdata(lsu_rdata));

`ifdef NETLIST
  // The netlist has no halt_sim/notify_retire -- those were simulation-only
  // RTL. Watch the CPU from outside: remember the last fetched instruction,
  // and use the inst_retired port to count instructions and spot ebreak.
  // a0 sits in anonymous flip-flops inside the netlist, so GOOD vs BAD trap
  // can't be told here -- judge by the program's own output.
  reg [31:0] fetched;
  always @(posedge clock) if (ifu_respValid) fetched <= ifu_rdata;

`ifdef __ICARUS__
  always @(posedge clock) begin
    if (!reset && inst_retired && fetched == 32'h00100073) begin
      $display("\nHIT EBREAK after %0d cycles (netlist: a0 not visible -- judge by the output above)", $time / 2);
      $finish;
    end
  end
`else
  import "DPI-C" function void notify_retire();
  import "DPI-C" function void netlist_ebreak();
  always @(posedge clock) begin
    if (!reset && inst_retired) begin
      notify_retire();
      if (fetched == 32'h00100073) netlist_ebreak();
    end
  end
`endif
`endif
endmodule

// One SimpleBus slave port: accept a request, answer it one cycle later with
// respValid held for exactly one cycle.
// Your CPU still drives reqValid=1 in the cycle it SEES respValid (its FSM only
// leaves the wait state on the next edge). That cycle is the tail of the request
// just answered, so it is ignored -- otherwise every access would happen twice,
// and every store to the UART would print its character twice.
module SimpleBusMem (
  input             clock,
  input             reset,
  input             reqValid,
  input      [31:0] addr,
  input             wen,
  input      [31:0] wdata,
  input      [3:0]  wmask,
  output reg        respValid,
  output reg [31:0] rdata
);
`ifndef __ICARUS__
  import "DPI-C" function int  pmem_read (input int raddr);
  import "DPI-C" function void pmem_write(input int waddr, input int wdata, input byte wmask);
`endif

  always @(posedge clock) begin
    if (reset) begin
      respValid <= 1'b0;
    end else if (respValid) begin
      respValid <= 1'b0;                   // tail of the request we just answered
    end else if (reqValid) begin
`ifdef __ICARUS__
      if (wen) $pmem_write(addr, wdata, wmask);       // VPI: Icarus has no DPI-C
      else     rdata <= $pmem_read(addr);
`else
      if (wen) pmem_write(addr, wdata, {4'b0, wmask});
      else     rdata <= pmem_read(addr);
`endif
      respValid <= 1'b1;
    end
  end

`ifdef __ICARUS__
  // Four-state check: once reset is over, the handshake must never be X.
  // An X here means some flip-flop that controls the bus was never reset.
  always @(posedge clock) begin
    if (!reset && ($isunknown(reqValid) || (reqValid && $isunknown(wen)))) begin
      $display("\n[icarus] %m: X on the bus handshake (reqValid=%b wen=%b) at time %0t",
               reqValid, wen, $time);
      $finish;
    end
  end
`endif
endmodule
