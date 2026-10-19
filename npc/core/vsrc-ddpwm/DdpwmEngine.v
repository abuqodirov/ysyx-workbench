// DdpwmEngine: dyadic digital PWM (DDPWM) for the minirv core slot of OSOC-DE.
// Method: Crovetti, Usmonov et al., IEEE TPEL 35(10), 2020 (eq. 11, Fig. 2).
// High time of each period: H = min(O + (n + b) * L, T), n = top N bits of
// DUTY, b = 0 (normal), m[M-1-k] with k = lowest set bit of c (dyadic), or
// c < m (thermometric). The slot has no pins, so the engine drives a pin with
// bus writes on the LSU port: GPIO PADOUT (PATH 0) or the SoC PWM (PATH 1).
// CSRs 0x7C0-0x7C8, timing and usage: rtl/minirv-ddpwm/README.md.
module DdpwmEngine #(
  parameter [15:0] SOC_SYNC = 16'd0  // SoC CNT in the clock after the EN response (run 09)
) (
  input         clock,
  input         reset,
  // CSR port (EXEC state of the core)
  input         csr_we,
  input  [11:0] csr_addr,
  input  [31:0] csr_wdata,
  output [31:0] csr_rdata,
  output        csr_hit,
  // bus port to the LSU arbiter (word writes only)
  output        bus_req,             // held until bus_done
  output [31:0] bus_addr,
  output [31:0] bus_wdata,
  input         bus_done,
  input         cpu_busy,            // a CPU fetch or data access is in progress
  output reg    reserve,             // the core must not start a new bus access
  output        sleep                // the core must not start a new fetch
);
  localparam [11:0] A_CTRL = 12'h7C0, A_CFG = 12'h7C1, A_PERIOD = 12'h7C2, A_STEP = 12'h7C3,
                    A_DUTY = 12'h7C4, A_OUT = 12'h7C5, A_RSVW = 12'h7C6, A_SLEEP = 12'h7C7,
                    A_STAT = 12'h7C8;
  localparam [2:0]  B_PADOUT = 3'd0, B_PCTRL = 3'd1, B_PSCR = 3'd2, B_CMP = 3'd3, B_CR = 3'd4;
  localparam [2:0]  S_OFF = 3'd0, S_CALC0 = 3'd1, S_CALC1 = 3'd2, S_INIT = 3'd3,
                    S_RUN = 3'd4, S_STOP = 3'd5;

  // CSRs
  reg        en, path, frame_latch, rsv_en, align, late;
  reg [1:0]  mode, sync;
  reg [3:0]  n_cfg, m_cfg;
  reg [4:0]  pin, spin;
  reg [1:0]  ch, sch;
  reg [15:0] period, step, offset, duty, rsvw, sleep_cnt;
  reg [31:0] out;

  reg  [2:0] state;
  wire       off = (state == S_OFF);
  wire       run = (state == S_RUN);

  // effective configuration (CFG, PERIOD and STEP are locked while running)
  wire [3:0] n_eff = (n_cfg < 4'd2) ? 4'd2 : (n_cfg > 4'd12) ? 4'd12 : n_cfg;
  wire [3:0] m_lim = (m_cfg > 4'd8) ? 4'd8 : m_cfg;
  wire [3:0] m_room = 4'd15 - n_eff + 4'd1;                  // 16 - N
  wire [3:0] m_eff = (m_lim > m_room) ? m_room : m_lim;
  wire [7:0] fmask = ~(8'hFF << m_eff);
  wire [15:0] t_eff = (period < 16'd16) ? 16'd16 : period;
  wire       sync_on = (sync == 2'd1 || sync == 2'd2) && (path ? (sch != ch) : (spin != pin));

  // period counter p, frame counter c
  reg [15:0] p;
  reg [7:0]  c;
  wire       wrap = run && (p == t_eff - 16'd1);

  // H = min(O + (n + b) * L, T), serial shift-add, one period ahead
  reg        calc_busy, calc_cur;                            // calc_cur: H of this period
  reg [12:0] calc_d;
  reg [16:0] calc_acc, calc_ls;                              // 0x10000 = saturated
  reg        calc_b;
  reg [15:0] h_cur, h_nxt, duty_frame;
  reg        b_cur, b_nxt, nxt_ready, launch;
  reg        calc_go;
  reg [7:0]  calc_c;

  wire [7:0]  cm = calc_c & fmask;
  wire        fstart = (cm == 8'd0);
  wire [15:0] dsel = (frame_latch && !fstart) ? duty_frame : duty;
  /* verilator lint_off UNUSEDSIGNAL */
  wire [15:0] dhi = dsel >> (5'd16 - {1'b0, n_eff});        // n
  wire [15:0] dw = dsel << n_eff;                            // m at the top
  /* verilator lint_on UNUSEDSIGNAL */
  wire [11:0] nval = dhi[11:0];
  wire [7:0]  mtop = dw[15:8];
  wire [7:0]  mrev = {mtop[0], mtop[1], mtop[2], mtop[3], mtop[4], mtop[5], mtop[6], mtop[7]};
  wire [7:0]  c_low1 = cm & (~cm + 8'd1);                    // lowest set bit of c
  wire        b_dy = |(mrev & c_low1);                       // m[M-1-k]
  wire [7:0]  mval = mtop >> (4'd8 - m_eff);
  wire        b_th = (cm < mval);
  wire        b_new = (mode == 2'd1) ? b_dy : (mode == 2'd2) ? b_th : 1'b0;

  wire [17:0] acc_sum = {1'b0, calc_acc} + (calc_d[0] ? {1'b0, calc_ls} : 18'd0);
  wire [16:0] acc_nxt = (acc_sum[17] | acc_sum[16]) ? 17'h10000 : acc_sum[16:0];
  wire [16:0] ls_nxt  = (calc_ls[16] | calc_ls[15]) ? 17'h10000 : {calc_ls[15:0], 1'b0};
  wire [15:0] h_val   = (acc_nxt >= {1'b0, t_eff}) ? t_eff : acc_nxt[15:0];
  wire        calc_fin = calc_busy && (calc_d[12:1] == 12'd0);

  // bus write port, one write at a time
  reg        inflight, rq_en, rq_last, written;
  reg [2:0]  rq_tgt;
  reg [1:0]  rq_ch;
  reg [31:0] rq_data;
  reg [31:0] last_pat;
  reg [2:0]  seq;                                            // start / stop sequence step
  // LATE: a CR write is safe if it lands before its own period starts, or in
  // that period before count wr_lo. The response is the latest landing time.
  reg        wr_chk, wr_need;
  reg [1:0]  wr_wraps;
  reg [15:0] wr_lo;

  // SoC path
  reg [15:0] cr_soc, sync_soc;                               // values now in the SoC CRs
  reg        cr_next;                                        // cr_soc is already next period's CR
  wire [15:0] cr_cur = t_eff - h_cur;
  wire [15:0] cr_nxt = t_eff - h_nxt;
  wire [15:0] w_open = (cr_soc > cr_nxt) ? cr_soc : cr_nxt;  // max(CR, CR')
  wire [15:0] lo_cur = (cr_soc < cr_cur) ? cr_soc : cr_cur;  // min(CR, CR')
  wire [15:0] lo_nxt = (cr_soc < cr_nxt) ? cr_soc : cr_nxt;
  wire [15:0] tm1 = t_eff - 16'd1;
  wire        last_of_frame  = ((c & fmask) == fmask);
  wire        last_of_frame1 = (((c + 8'd1) & fmask) == fmask);
  wire [15:0] sync_cr_cur = (sync == 2'd1 || last_of_frame)  ? tm1 : 16'hFFFF;
  wire [15:0] sync_cr_nxt = (sync == 2'd1 || last_of_frame1) ? tm1 : 16'hFFFF;
  wire        soc_catch = !cr_next && (cr_cur != cr_soc);
  wire        sync_wr  = sync_on && (sync_soc != sync_cr_cur);
  wire        soc_nrm   = !cr_next && !soc_catch && nxt_ready && (cr_nxt != cr_soc);
  wire        soc_nrm_go = soc_nrm && (p >= w_open);

  // GPIO path
  reg        stog;                                           // period-mode sync level
  wire       lvl = align ? (p >= t_eff - h_cur) : (p < h_cur);
  wire       slvl = (sync == 2'd1) ? stog : ((c & fmask) == 8'd0);
  wire [31:0] pin_bit  = 32'd1 << pin;
  wire [31:0] spin_bit = sync_on ? (32'd1 << spin) : 32'd0;
  wire [31:0] pat_off  = out & ~pin_bit & ~spin_bit;
  wire [31:0] pattern  = pat_off | (lvl ? pin_bit : 32'd0) | (slvl ? spin_bit : 32'd0);
  wire        gpio_want = run && !path && (!written || pattern != last_pat);

  // next bus write
  reg        iss;
  reg [2:0]  iss_tgt;
  reg [1:0]  iss_ch;
  reg [31:0] iss_data;
  reg        iss_en, iss_last;
  always @(*) begin
    iss = 1'b0; iss_tgt = B_CR; iss_ch = ch; iss_data = 32'd0; iss_en = 1'b0; iss_last = 1'b0;
    if (!inflight) begin
      case (state)
        S_INIT: if (path) case (seq)                         // SoC PWM start
          3'd0: begin iss = 1'b1; iss_data = {16'd0, cr_cur}; end
          3'd1: begin iss = 1'b1; iss_ch = sch; iss_data = {16'd0, sync_cr_cur}; end
          3'd2: begin iss = 1'b1; iss_tgt = B_PCTRL; iss_data = 32'd4; end      // CLR
          3'd3: begin iss = 1'b1; iss_tgt = B_PSCR;  iss_data = 32'd0; end
          3'd4: begin iss = 1'b1; iss_tgt = B_CMP;   iss_data = {16'd0, t_eff}; end
          default: begin                                     // EN on a free bus
            iss = reserve && !cpu_busy; iss_tgt = B_PCTRL; iss_data = 32'd2; iss_en = 1'b1;
          end
        endcase
        S_STOP: if (!path) begin
          iss = 1'b1; iss_tgt = B_PADOUT; iss_data = pat_off; iss_last = 1'b1;
        end else case (seq)
          3'd0:    begin iss = 1'b1; iss_data = 32'hFFFF; end
          3'd1:    begin iss = 1'b1; iss_ch = sch; iss_data = 32'hFFFF; end
          default: begin iss = 1'b1; iss_tgt = B_PCTRL; iss_data = 32'd0; iss_last = 1'b1; end
        endcase
        S_RUN: if (!path) begin
          iss = gpio_want; iss_tgt = B_PADOUT; iss_data = pattern;
        end else if (soc_catch) begin                        // not in the last clock
          iss = !wrap; iss_data = {16'd0, cr_cur};
        end else if (sync_wr) begin
          iss = !wrap; iss_ch = sch; iss_data = {16'd0, sync_cr_cur};
        end else if (soc_nrm_go) begin
          iss = 1'b1; iss_data = {16'd0, cr_nxt};
        end
        default: ;
      endcase
    end
  end

  always @(*) begin
    calc_go = 1'b0; calc_c = c + 8'd1;
    if (off && en)                         begin calc_go = 1'b1; calc_c = 8'd0; end
    else if (state == S_CALC0 && calc_fin) begin calc_go = 1'b1; calc_c = 8'd1; end
    else if (run && launch && !wrap)       calc_go = 1'b1;
  end

  wire [15:0] gpio_e2 = align ? (t_eff - h_cur) : h_cur;     // edge inside the period
  wire [15:0] gpio_to = (p < gpio_e2) ? (gpio_e2 - p) : (t_eff - p);
  wire [15:0] soc_wo  = (w_open < t_eff) ? w_open : t_eff;

  always @(posedge clock) begin
    if (reset) begin
      en <= 1'b0; path <= 1'b0; frame_latch <= 1'b0; rsv_en <= 1'b0; align <= 1'b0; late <= 1'b0;
      mode <= 2'd0; sync <= 2'd0; n_cfg <= 4'd0; m_cfg <= 4'd0; pin <= 5'd0; spin <= 5'd0;
      ch <= 2'd0; sch <= 2'd0; period <= 16'd0; step <= 16'd0; offset <= 16'd0; duty <= 16'd0;
      rsvw <= 16'd0; sleep_cnt <= 16'd0; out <= 32'd0;
      state <= S_OFF; p <= 16'd0; c <= 8'd0;
      calc_busy <= 1'b0; calc_cur <= 1'b0; calc_d <= 13'd0; calc_acc <= 17'd0; calc_ls <= 17'd0;
      calc_b <= 1'b0; h_cur <= 16'd0; h_nxt <= 16'd0; duty_frame <= 16'd0;
      b_cur <= 1'b0; b_nxt <= 1'b0; nxt_ready <= 1'b0; launch <= 1'b0;
      inflight <= 1'b0; rq_en <= 1'b0; rq_last <= 1'b0; written <= 1'b0;
      rq_tgt <= 3'd0; rq_ch <= 2'd0; rq_data <= 32'd0; last_pat <= 32'd0; seq <= 3'd0;
      wr_chk <= 1'b0; wr_need <= 1'b0; wr_wraps <= 2'd0; wr_lo <= 16'd0;
      cr_soc <= 16'd0; sync_soc <= 16'd0; cr_next <= 1'b0; stog <= 1'b0;
      reserve <= 1'b0;
    end else begin
      // CSR writes
      if (csr_we) case (csr_addr)
        A_CTRL: begin
          en <= csr_wdata[0]; mode <= csr_wdata[3:2]; frame_latch <= csr_wdata[4];
          rsv_en <= csr_wdata[5];
          if (off) begin path <= csr_wdata[1]; sync <= csr_wdata[7:6]; align <= csr_wdata[8]; end
        end
        A_CFG: if (off) begin
          n_cfg <= csr_wdata[3:0]; m_cfg <= csr_wdata[7:4]; pin <= csr_wdata[12:8];
          spin <= csr_wdata[17:13]; ch <= csr_wdata[19:18]; sch <= csr_wdata[21:20];
        end
        A_PERIOD: if (off) period <= csr_wdata[15:0];
        A_STEP:   if (off) begin step <= csr_wdata[15:0]; offset <= csr_wdata[31:16]; end
        A_DUTY:   duty <= csr_wdata[15:0];
        A_OUT:    out <= csr_wdata;
        A_RSVW:   rsvw <= csr_wdata[15:0];
        default: ;
      endcase

      // H calculation (a new start may come in the clock of a finish)
      if (calc_busy) begin
        calc_acc <= acc_nxt; calc_ls <= ls_nxt; calc_d <= calc_d >> 1;
        if (calc_fin) begin
          calc_busy <= 1'b0;
          if (calc_cur) begin h_cur <= h_val; b_cur <= calc_b; end
          else          begin h_nxt <= h_val; b_nxt <= calc_b; nxt_ready <= 1'b1; end
        end
      end
      if (calc_go) begin
        calc_busy <= 1'b1; calc_cur <= off;
        calc_d <= {1'b0, nval} + {12'd0, b_new};
        calc_acc <= {1'b0, offset}; calc_ls <= {1'b0, step}; calc_b <= b_new;
        if (fstart) duty_frame <= duty;
        if (!off) nxt_ready <= 1'b0;
      end

      // period and frame counters
      if (run) begin
        if (wrap) begin
          p <= 16'd0; c <= c + 8'd1; h_cur <= h_nxt; b_cur <= b_nxt;
          nxt_ready <= 1'b0;
          launch <= 1'b1; cr_next <= 1'b0; stog <= ~stog;
          if (wr_wraps != 2'd3) wr_wraps <= wr_wraps + 2'd1;
        end else begin
          p <= p + 16'd1;
          if (calc_go) launch <= 1'b0;
        end
        if (path && !cr_next && !soc_catch && nxt_ready && cr_nxt == cr_soc && !wrap)
          cr_next <= 1'b1;                                   // next period needs no write
      end

      // sleep counts period starts
      if (csr_we && csr_addr == A_SLEEP) sleep_cnt <= csr_wdata[15:0];
      else if (off)                       sleep_cnt <= 16'd0;
      else if (wrap && sleep_cnt != 16'd0) sleep_cnt <= sleep_cnt - 16'd1;

      // bus writes
      if (iss) begin
        inflight <= 1'b1; rq_tgt <= iss_tgt; rq_ch <= iss_ch; rq_data <= iss_data;
        rq_en <= iss_en; rq_last <= iss_last; wr_chk <= 1'b0;
        wr_wraps <= wrap ? 2'd1 : 2'd0;
        if (state == S_INIT) begin
          seq <= (seq == 3'd0 && !sync_on) ? 3'd2 : seq + 3'd1;
          if (seq == 3'd0) cr_soc <= cr_cur;
          if (seq == 3'd1) sync_soc <= sync_cr_cur;
        end
        if (state == S_STOP) seq <= (seq == 3'd0 && !sync_on) ? 3'd2 : seq + 3'd1;
        if (run && !path) begin last_pat <= pattern; written <= 1'b1; end
        if (run && path) begin
          wr_chk <= 1'b1;
          if (soc_catch) begin
            cr_soc <= cr_cur; wr_need <= 1'b0; wr_lo <= lo_cur;
          end else if (sync_wr) begin
            sync_soc <= sync_cr_cur; wr_need <= 1'b0; wr_lo <= tm1;
          end else begin                                     // in the wrap clock the next
            cr_soc <= cr_nxt; cr_next <= !wrap;              // period is already this one
            wr_need <= 1'b1; wr_lo <= lo_nxt;
          end
        end
      end else if (inflight && bus_done) begin
        inflight <= 1'b0;
        if (run && wr_chk && (wr_wraps > {1'b0, wr_need} || (wr_wraps == {1'b0, wr_need} && p >= wr_lo)))
          late <= 1'b1;
      end
      if (csr_we && csr_addr == A_STAT) late <= 1'b0;

      case (state)
        S_OFF:   if (en) begin state <= S_CALC0; c <= 8'd0; end
        S_CALC0: if (!en) state <= S_OFF; else if (calc_fin) state <= S_CALC1;
        S_CALC1: if (!en) state <= S_OFF;
                 else if (calc_fin) begin state <= S_INIT; seq <= 3'd0; end
        S_INIT:  if (!en) begin state <= S_STOP; seq <= 3'd0; end
                 else if (path && inflight && bus_done && rq_en) begin
                   state <= S_RUN; p <= SOC_SYNC; launch <= 1'b0; cr_next <= 1'b0;
                 end else if (!path && reserve && !cpu_busy) begin   // GPIO: start on a free bus
                   state <= S_RUN; p <= 16'd0; launch <= 1'b0; written <= 1'b0; stog <= 1'b1;
                 end
        S_RUN:   if (!en) begin state <= S_STOP; seq <= 3'd0; end
        S_STOP:  if (inflight && bus_done && rq_last) state <= S_OFF;
        default: state <= S_OFF;
      endcase

      // reservation (registered: it acts one clock later)
      reserve <= (state == S_INIT) ||
                 (rsv_en && run && (iss || inflight ||
                   (!path && gpio_to <= rsvw) ||
                   (path && ((soc_nrm && soc_wo - p <= rsvw) ||
                             (sync_on && sync_cr_nxt != sync_cr_cur && t_eff - p <= rsvw)))));
    end
  end

  assign sleep = (sleep_cnt != 16'd0);

  assign bus_req   = inflight;
  assign bus_wdata = rq_data;
  reg [31:0] addr;
  always @(*) begin
    case (rq_tgt)
      B_PADOUT: addr = 32'h1010_0008;
      B_PCTRL:  addr = 32'h1010_6000;
      B_PSCR:   addr = 32'h1010_6004;
      B_CMP:    addr = 32'h1010_600C;
      default:  addr = {24'h101060, 3'b000, 1'b1, rq_ch, 2'b00};  // CR0-CR3
    endcase
  end
  assign bus_addr = addr;

  reg [31:0] rd;
  always @(*) begin
    case (csr_addr)
      A_CTRL:   rd = {23'd0, align, sync, rsv_en, frame_latch, mode, path, en};
      A_CFG:    rd = {10'd0, sch, ch, spin, pin, m_cfg, n_cfg};
      A_PERIOD: rd = {16'd0, period};
      A_STEP:   rd = {offset, step};
      A_DUTY:   rd = {16'd0, duty};
      A_OUT:    rd = out;
      A_RSVW:   rd = {16'd0, rsvw};
      A_SLEEP:  rd = {16'd0, sleep_cnt};
      A_STAT:   rd = {h_cur, c, 2'd0, run, inflight | iss, sleep, b_cur, late, !off};
      default:  rd = 32'd0;
    endcase
  end
  assign csr_rdata = rd;
  assign csr_hit   = (csr_addr[11:4] == 8'h7C) && (csr_addr[3:0] <= 4'd8);

`ifndef SYNTHESIS
  // simulation trace for scripts/check-ddpwm-trace.py
  reg [63:0] sim_cycle, sim_issue;
  always @(posedge clock) begin
    if (reset) sim_cycle <= 64'd0;
    else begin
      sim_cycle <= sim_cycle + 64'd1;
      if (csr_we && csr_hit)
        $display("DDPWM csr 0x%03x = 0x%08x cycle=%0d", csr_addr, csr_wdata, sim_cycle);
      if (calc_go)
        $display("DDPWM calc c=%0d duty=0x%04x cycle=%0d", calc_c, dsel, sim_cycle);
      if ((wrap || (state == S_INIT && en && !path && reserve && !cpu_busy) ||
           (state == S_INIT && en && path && inflight && bus_done && rq_en)))
        $display("DDPWM period c=%0d h=%0d b=%0d cycle=%0d t=%0t", wrap ? c + 8'd1 : 8'd0,
                 wrap ? h_nxt : h_cur, wrap ? b_nxt : b_cur, sim_cycle + 64'd1, $time);
      if (iss) sim_issue <= sim_cycle;
      if (inflight && bus_done)
        $display("DDPWM write 0x%08x = 0x%08x issue=%0d response=%0d late=%0d", addr, rq_data,
                 sim_issue, sim_cycle,
                 (run && wr_chk && (wr_wraps > {1'b0, wr_need} || (wr_wraps == {1'b0, wr_need} && p >= wr_lo))) ? 1 : 0);
    end
  end
`endif
endmodule
