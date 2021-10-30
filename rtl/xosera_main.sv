// xosera_main.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
// This project would not be possible without learning from the following
// open projects (and many others, no doubt):
//
// YaGraphCon       - http://www.frank-buss.de/yagraphcon/
// yavga            - https://opencores.org/projects/yavga
// f32c             - https://github.com/f32c
// up5k_vga         - https://github.com/emeb/up5k_vga
// icestation-32    - https://github.com/dan-rodrigues/icestation-32Tanger
// ice40-playground - https://github.com/smunaut/ice40-playground
// Project-F        - https://github.com/projf/projf-explore
//
// Also the following web sites:
// Hamsterworks     - https://web.archive.org/web/20190119005744/http://hamsterworks.co.nz/mediawiki/index.php/Main_Page
//                    (Archived, but not forgotten - Thanks Mike Fields)
// John's FPGA Page - http://members.optushome.com.au/jekent/FPGA.htm
// FPGA4Fun         - https://www.fpga4fun.com/
// Nandland         - https://www.nandland.com/
// Project-F        - https://projectf.io/
// Alchrity         - https://alchitry.com/
//
// 1BitSquared Discord server has also been welcoming and helpful - https://1bitsquared.com/pages/chat
//
// Special thanks to everyone involved with the IceStorm/Yosys/NextPNR (etc.) open source FPGA projects.
// Consider supporting open source FPGA tool development: https://www.patreon.com/fpga_dave

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

module xosera_main(
           input  wire logic         clk,                    // pixel clock
           input  wire logic         bus_cs_n_i,             // register select strobe (active low)
           input  wire logic         bus_rd_nwr_i,           // 0 = write, 1 = read
           input  wire logic [3:0]   bus_reg_num_i,          // register number
           input  wire logic         bus_bytesel_i,          // 0 = even byte, 1 = odd byte
           input  wire logic [7:0]   bus_data_i,             // 8-bit data bus input
           output logic      [7:0]   bus_data_o,             // 8-bit data bus output
           output logic              bus_intr_o,             // Xosera CPU interrupt strobe
           output logic      [3:0]   red_o, green_o, blue_o, // RGB 4-bit color outputs
           output logic              hsync_o, vsync_o,       // horizontal and vertical sync
           output logic              dv_de_o,                // pixel visible (aka display enable)
           output logic              audio_l_o, audio_r_o,   // left and right audio PWM output
           output logic              reconfig_o,             // reconfigure iCE40 from flash
           output logic      [1:0]   boot_select_o,          // reconfigure congigureation number (0-3)
           input  wire logic         reset_i                 // reset signal
       );

// video generation
logic           vgen_vram_sel;      // video gen vram select (read only)
logic [15:0]    vgen_vram_addr;     // video gen vram addr
logic [15:0]    vgen_reg_data_out;  // video gen register output

logic           dv_de_1;            // display enable delayed
logic           hsync_1;            // hsync delayed
logic           vsync_1;            // vsync delayed

logic  [7:0]    color_index     /* verilator public */; // COLORMEM index
logic [15:0]    color_xrgb      /* verilator public */; // COLORMEM XRGB output

// register interface vram/xr access
logic           regs_vram_sel   /* verilator public */;
logic           regs_vram_ack   /* verilator public */;
logic           regs_xr_sel     /* verilator public */;
logic           regs_xr_ack     /* verilator public */;
logic           regs_wr         /* verilator public */;
logic  [3:0]    regs_wr_mask    /* verilator public */;
logic [15:0]    regs_vram_addr  /* verilator public */;

// blit vram/xr access
logic           blit_vram_sel   /* verilator public */  = 1'b0;
logic           blit_vram_ack   /* verilator public */;
logic           blit_wr         /* verilator public */  = 1'b0;
logic  [3:0]    blit_wr_mask    /* verilator public */  = 4'b0;
logic [15:0]    blit_vram_addr  /* verilator public */  = 16'b0;
logic [15:0]    blit_vram_data  /* verilator public */  = 16'b0;

// draw vram/xr access
logic           draw_vram_sel   /* verilator public */  = 1'b0;
logic           draw_vram_ack   /* verilator public */;
logic           draw_xr_sel     /* verilator public */  = 1'b0;
logic           draw_xr_ack     /* verilator public */  = 1'b0;
logic           draw_wr         /* verilator public */  = 1'b0;
logic  [3:0]    draw_wr_mask    /* verilator public */  = 4'b0;
logic [15:0]    draw_vram_addr  /* verilator public */  = 16'b0;
logic [15:0]    draw_vram_data  /* verilator public */  = 16'b0;

logic [15:0] regs_addr      /* verilator public */;     // register interface VRAM/XR addr
logic [15:0] regs_data_out  /* verilator public */;     // register interface bus VRAM/XR data write

logic   regs_vgen_reg_sel   /* verilator public */;
logic   regs_vgen_reg_wr    /* verilator public */;

logic   regs_tilemem_sel    /* verilator public */;
logic   regs_tilemem_wr     /* verilator public */;

logic   regs_colormem_sel   /* verilator public */;
logic   regs_colormem_wr    /* verilator public */;

logic   regs_coppermem_sel  /* verilator public */;
logic   regs_coppermem_wr   /* verilator public */;

logic   regs_spritemem_sel  /* verilator public */;
logic   regs_spritemem_wr   /* verilator public */;

assign  regs_vgen_reg_sel   = regs_xr_sel && !regs_addr[15];
assign  regs_colormem_sel   = regs_xr_sel && regs_addr[15] && (regs_addr[13:12] == xv::XR_COLOR_MEM[13:12]);
assign  regs_tilemem_sel    = regs_xr_sel && regs_addr[15] && (regs_addr[13:12] == xv::XR_TILE_MEM[13:12]);
assign  regs_coppermem_sel  = regs_xr_sel && regs_addr[15] && (regs_addr[13:12] == xv::XR_COPPER_MEM[13:12]);
assign  regs_spritemem_sel  = regs_xr_sel && regs_addr[15] && (regs_addr[13:12] == xv::XR_SPRITE_MEM[13:12]);

assign  regs_vgen_reg_wr    = regs_vgen_reg_sel && regs_wr;
assign  regs_tilemem_wr     = regs_tilemem_sel && regs_wr;
assign  regs_colormem_wr    = regs_colormem_sel && regs_wr;
assign  regs_spritemem_wr   = regs_spritemem_sel && regs_wr;
assign  regs_coppermem_wr   = regs_coppermem_sel && regs_wr;

// Copper
logic [15:0] copp_wr_addr       /* verilator public */;
logic [15:0] copp_data_out      /* verilator public */;
logic        copp_xr_wr_sel     /* verilator public */;

logic        copp_vgen_reg_wr   /* verilator public */;
logic        copp_colormem_wr   /* verilator public */;
logic        copp_tilemem_wr    /* verilator public */;
logic        copp_coppermem_wr  /* verilator public */;

`ifndef COPPER_DISABLE
assign  copp_vgen_reg_wr    = copp_xr_wr_sel && !copp_wr_addr[15];
assign  copp_colormem_wr    = copp_xr_wr_sel && copp_wr_addr[15] && (copp_wr_addr[13:12] == xv::XR_COLOR_MEM[13:12]);
assign  copp_tilemem_wr     = copp_xr_wr_sel && copp_wr_addr[15] && (copp_wr_addr[13:12] == xv::XR_TILE_MEM[13:12]);
assign  copp_coppermem_wr   = copp_xr_wr_sel && copp_wr_addr[15] && (copp_wr_addr[13:12] == xv::XR_COPPER_MEM[13:12]);
`else
assign  copp_vgen_reg_wr    = 1'b0;
assign  copp_colormem_wr    = 1'b0;
assign  copp_tilemem_wr     = 1'b0;
assign  copp_coppermem_wr   = 1'b0;
`endif

`ifndef COPPER_DISABLE
logic        coppermem_wr_in;
logic [10:0] coppermem_wr_addr_in;
logic [15:0] coppermem_wr_data_in;
`endif

logic        colormem_wr_in;
logic  [7:0] colormem_wr_addr_in;
logic [15:0] colormem_wr_data_in;

logic        tilemem_wr_in;
logic [11:0] tilemem_wr_addr_in;
logic [15:0] tilemem_wr_data_in;

logic        xr_reg_wr_in;
logic [4:0]  xr_reg_wr_addr_in;
logic [15:0] xr_reg_wr_data_in;

`ifndef COPPER_DISABLE
assign coppermem_wr_in          = regs_coppermem_wr  || copp_coppermem_wr;
assign coppermem_wr_addr_in     = regs_coppermem_wr   ? regs_addr[10:0] : copp_wr_addr[10:0];
assign coppermem_wr_data_in     = regs_coppermem_wr   ? regs_data_out   : copp_data_out;
`endif

assign colormem_wr_in           = regs_colormem_wr || copp_colormem_wr;
assign colormem_wr_addr_in      = regs_colormem_wr  ? regs_addr[7:0]  : copp_wr_addr[7:0];
assign colormem_wr_data_in      = regs_colormem_wr  ? regs_data_out   : copp_data_out;

assign tilemem_wr_in            = regs_tilemem_wr    || copp_tilemem_wr;
assign tilemem_wr_addr_in       = regs_tilemem_wr     ? regs_addr[11:0]  : copp_wr_addr[11:0];
assign tilemem_wr_data_in       = regs_tilemem_wr     ? regs_data_out    : copp_data_out;

assign xr_reg_wr_in             = regs_vgen_reg_wr   || copp_vgen_reg_wr;
assign xr_reg_wr_addr_in        = regs_vgen_reg_wr    ? regs_addr[4:0]   : copp_wr_addr[4:0];
assign xr_reg_wr_data_in        = regs_vgen_reg_wr    ? regs_data_out    : copp_data_out;

`ifndef COPPER_DISABLE
// Separate strobes for even & odd copper RAM
logic           coppermem_e_wr_in;
logic           coppermem_o_wr_in;
assign          coppermem_e_wr_in   = coppermem_wr_in & ~coppermem_wr_addr_in[0];
assign          coppermem_o_wr_in   = coppermem_wr_in &  coppermem_wr_addr_in[0];

logic           copp_reg_wr;
logic [15:0]    copp_reg_data;

logic [xv::COPPERMEM_AWIDTH-1:0] copper_pc;
logic           coppermem_rd_en;
logic [15:0]    coppermem_e_rd_data_out;
logic [15:0]    coppermem_o_rd_data_out;
//logic [15:0]    copp_reg_data_out;
`endif

/* verilator lint_off UNUSED */
logic [10:0]    video_h_count;
logic [10:0]    video_v_count;
/* verilator lint_on UNUSED */

//  VRAM read output data (for vgen, regs, blit, draw)
logic [15:0]    vram_data_out   /* verilator public */;

// interrupt management signals
logic  [3:0]    intr_mask;          // true for each enabled interrupt
logic  [3:0]    intr_status;        // pending interrupt status
logic  [3:0]    intr_signal;        // interrupt signalled by Copper (or CPU)
logic  [3:0]    intr_clear;         // interrupt cleared by CPU

`ifdef BUS_DEBUG_SIGNALS
logic           dbug_cs_strobe;     // debug "ack" bus strobe
`endif

logic           tilemem_rd_en       /* verilator public */;
logic [11:0]    tilemem_addr        /* verilator public */; // 12-bit word address
logic [15:0]    tilemem_data_out    /* verilator public */;

logic           spritemem_rd_en     /* verilator public */;
logic  [7:0]    spritemem_addr      /* verilator public */; // 8-bit word address
logic [15:0]    spritemem_data_out  /* verilator public */;

`ifdef BUS_DEBUG_SIGNALS
assign audio_l_o = dbug_cs_strobe;  // debug to see when CS noticed
assign audio_r_o = regs_xr_sel;     // debug to see when XR bus selected
`else
// TODO: audio generation
assign audio_l_o = 1'b0;
assign audio_r_o = 1'b0;
`endif

// register interface for CPU access
reg_interface reg_interface(
    // bus
    .bus_cs_n_i(bus_cs_n_i),            // bus chip select
    .bus_rd_nwr_i(bus_rd_nwr_i),        // 0=write, 1=read
    .bus_reg_num_i(bus_reg_num_i),      // register number (0-15)
    .bus_bytesel_i(bus_bytesel_i),      // 0=even byte, 1=odd byte
    .bus_data_i(bus_data_i),            // 8-bit data bus input
    .bus_data_o(bus_data_o),            // 8-bit data bus output
    // VRAM/XR
    .vram_ack_i(regs_vram_ack),         // register interface ack (after reg read/write cycle)
    .xr_ack_i(regs_xr_ack),             // register interface ack (after reg read/write cycle)
    .regs_vram_sel_o(regs_vram_sel),    // register interface vram select
    .regs_xr_sel_o(regs_xr_sel),        // register interface aux memory select
    .regs_wr_o(regs_wr),                // register interface write
    .regs_wrmask_o(regs_wr_mask),       // vram nibble masks
    .regs_addr_o(regs_addr),            // vram/aux address
    .regs_data_i(vram_data_out),        // 16-bit word read from vram
    .regs_data_o(regs_data_out),        // 16-bit word write to aux/vram
    .xr_data_i(vgen_reg_data_out),      // 16-bit word read from aux
    //
    .busy_i(1'b0),                      // TODO: blit/draw engine busy
    // reconfig
    .reconfig_o(reconfig_o),
    .boot_select_o(boot_select_o),
    // interrupts
    .intr_mask_o(intr_mask),            // set with write to SYS_CTRL
    .intr_clear_o(intr_clear),          // strobe with write to TIMER
`ifdef BUS_DEBUG_SIGNALS
    .bus_ack_o(dbug_cs_strobe),         // debug "ack" bus strobe
`endif
    .reset_i(reset_i),
    .clk(clk)
);

//  video generation
video_gen video_gen(
    .vgen_reg_wr_i(xr_reg_wr_in),
    .vgen_reg_num_r_i(regs_addr[4:0]),
    .vgen_reg_num_w_i(xr_reg_wr_addr_in),
    .vgen_reg_data_i(xr_reg_wr_data_in),
    .vgen_reg_data_o(vgen_reg_data_out),
    .intr_status_i(intr_status),        // status read from VID_CTRL
    .intr_signal_o(intr_signal),        // signaled by write to VID_CTRL
    .vram_sel_o(vgen_vram_sel),
    .vram_addr_o(vgen_vram_addr),
    .vram_data_i(vram_data_out),
    .tilemem_sel_o(tilemem_rd_en),
    .tilemem_addr_o(tilemem_addr),
    .tilemem_data_i(tilemem_data_out),
    .spritemem_sel_o(spritemem_rd_en),
    .spritemem_addr_o(spritemem_addr),
    .spritemem_data_i(spritemem_data_out),
    .color_index_o(color_index),
    .hsync_o(hsync_1),
    .vsync_o(vsync_1),
    .dv_de_o(dv_de_1),
`ifndef COPPER_DISABLE
    .copp_reg_wr_o(copp_reg_wr),
    .copp_reg_data_o(copp_reg_data),
    .h_count_o(video_h_count),
    .v_count_o(video_v_count),
`endif
    .reset_i(reset_i),
    .clk(clk)
);

// VRAM memory arbitration
vram_arb vram_arb(
    // video gen
    .vram_data_o(vram_data_out),
    .vgen_sel_i(vgen_vram_sel),
    .vgen_addr_i(vgen_vram_addr),
    // register interface
    .regs_sel_i(regs_vram_sel),
    .regs_ack_o(regs_vram_ack),
    .regs_wr_i(regs_wr & regs_vram_sel),
    .regs_wr_mask_i(regs_wr_mask),
    .regs_addr_i(regs_addr),
    .regs_data_i(regs_data_out),
    // TODO: 2D blit
    .blit_sel_i(blit_vram_sel),
    .blit_ack_o(blit_vram_ack),
    .blit_wr_i(blit_wr & blit_vram_sel),
    .blit_wr_mask_i(blit_wr_mask),
    .blit_addr_i(blit_vram_addr),
    .blit_data_i(blit_vram_data),
    // TODO: polygon draw
    .draw_sel_i(draw_vram_sel),
    .draw_ack_o(draw_vram_ack),
    .draw_wr_i(draw_wr & regs_vram_sel),
    .draw_wr_mask_i(draw_wr_mask),
    .draw_addr_i(draw_vram_addr),
    .draw_data_i(draw_vram_data),

    .clk(clk)
);

// video color RAM
colormem #(
    .AWIDTH(xv::COLORMEM_AWIDTH)
    ) colormem(
    .clk(clk),
    .rd_en_i(1'b1),
    .rd_address_i(color_index),
    .rd_data_o(color_xrgb),
    .wr_clk(clk),
    .wr_en_i(colormem_wr_in),
    .wr_address_i(colormem_wr_addr_in),
    .wr_data_i(colormem_wr_data_in)
);

//  16-bit x 4KB tile memory
tilemem #(
    .AWIDTH(xv::TILEMEM_AWIDTH)
    )
    tilemem (
    .clk(clk),
    .rd_en_i(tilemem_rd_en),
    .rd_address_i(tilemem_addr),
    .rd_data_o(tilemem_data_out),
    .wr_clk(clk),
    .wr_en_i(tilemem_wr_in),
    .wr_address_i(tilemem_wr_addr_in),
    .wr_data_i(tilemem_wr_data_in)
);

// cursor sprite RAM
spritemem #(
    .AWIDTH(xv::SPRITEMEM_AWIDTH)
    )
    spritemem(
    .clk(clk),
    .rd_en_i(spritemem_rd_en),
    .rd_address_i(spritemem_addr),
    .rd_data_o(spritemem_data_out),
    .wr_clk(clk),
    .wr_en_i(regs_spritemem_wr),
    .wr_address_i(regs_addr[7:0]),
    .wr_data_i(regs_data_out)
);

`ifndef COPPER_DISABLE
// Copper
copper copper(
    .clk(clk),
    .reset_i(reset_i),
    .xr_ram_wr_en_o(copp_xr_wr_sel),
    .xr_ram_wr_addr_o(copp_wr_addr),
    .xr_ram_wr_data_o(copp_data_out),
    .coppermem_rd_addr_o(copper_pc),
    .coppermem_rd_en_o(coppermem_rd_en),
    .coppermem_e_rd_data_i(coppermem_e_rd_data_out),
    .coppermem_o_rd_data_i(coppermem_o_rd_data_out),
    .regs_xr_reg_sel_i(regs_vgen_reg_sel),
    .regs_tilemem_sel_i(regs_tilemem_sel),
    .regs_colormem_sel_i(regs_colormem_sel),
    .regs_coppermem_sel_i(regs_coppermem_sel),
    .copp_reg_wr_i(copp_reg_wr),
    .copp_reg_data_i(copp_reg_data),
    .h_count_i(video_h_count),
    .v_count_i(video_v_count)
);

// Copper RAM (Even word)
coppermem #(
    .AWIDTH(xv::COPPERMEM_AWIDTH)
    ) coppermem_e(
    .clk(clk),
    .rd_en_i(coppermem_rd_en),
    .rd_address_i(copper_pc),
    .rd_data_o(coppermem_e_rd_data_out),
    .wr_clk(clk),
    .wr_en_i(coppermem_e_wr_in),
    .wr_address_i(coppermem_wr_addr_in[10:1]),
    .wr_data_i(coppermem_wr_data_in)
);

// Copper RAM (Odd word)
coppermem #(
    .AWIDTH(xv::COPPERMEM_AWIDTH)
    ) coppermem_o(
    .clk(clk),
    .rd_en_i(coppermem_rd_en),
    .rd_address_i(copper_pc),
    .rd_data_o(coppermem_o_rd_data_out),
    .wr_clk(clk),
    .wr_en_i(coppermem_o_wr_in),
    .wr_address_i(coppermem_wr_addr_in[10:1]),
    .wr_data_i(coppermem_wr_data_in)
);
`endif

// color RAM lookup (delays video 1 cycle for BRAM)
always_ff @(posedge clk) begin
    vsync_o     <= vsync_1;
    hsync_o     <= hsync_1;
    dv_de_o     <= dv_de_1;
    if (dv_de_1) begin
        red_o       <= color_xrgb[11:8];
        green_o     <= color_xrgb[7:4];
        blue_o      <= color_xrgb[3:0];
    end else begin
        red_o       <= 4'h0;
        green_o     <= 4'h0;
        blue_o      <= 4'h0;
    end
end

// interrupt handling
always_ff @(posedge clk) begin
    if (reset_i) begin
        bus_intr_o  <= 1'b0;
        intr_status <= 4'b0;
    end else begin
        // signal a bus interrupt if not masked and not set in status and
        if ((intr_signal & intr_mask & (~intr_status)) != 4'b0) begin
            bus_intr_o  <= 1'b1;
        end else begin
            bus_intr_o  <= 1'b0;
        end
        // remember interrupt signal and clear cleared interrupts
        intr_status <= (intr_status | intr_signal) & (~intr_clear);
    end
end

endmodule
`default_nettype wire               // restore default
