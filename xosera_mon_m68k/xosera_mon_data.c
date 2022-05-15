/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *  __ __
 * |  |  |___ ___ ___ ___ ___
 * |-   -| . |_ -| -_|  _| .'|
 * |__|__|___|___|___|_| |__,|
 *
 * Xark's Open Source Enhanced Retro Adapter
 *
 * - "Not as clumsy or random as a GPU, an embedded retro
 *    adapter for a more civilized age."
 *
 * ------------------------------------------------------------
 * Copyright (c) 2021-2022 Xark
 * MIT License
 *
 * Xosera rosco_m68k mon register data file
 * ------------------------------------------------------------
 */

#include "xosera_mon_m68k.h"

// Xosera XR Memory Regions (size in 16-bit words)
const addr_range_t xr_mem[] = {
    {"XR_VID_CTRL", 0x00, 0x1},         // (R /W) display control and border color index
    {"XR_COPP_CTRL", 0x01, 0x1},        // (R /W) display synchronized coprocessor control
    {"XR_AUD_CTRL", 0x02, 0x1},         // (- /-) TODO: audio channel control
    {"XR_UNUSED_03", 0x03, 0x1},        // (- /-) TODO: unused XR 03
    {"XR_VID_LEFT", 0x04, 0x1},         // (R /W) left edge of active display window (typically 0)
    {"XR_VID_RIGHT", 0x05, 0x1},        // (R /W) right edge of active display window +1 (typically 640 or 848)
    {"XR_UNUSED_06", 0x06, 0x1},        // (- /-) TODO: unused XR 06
    {"XR_UNUSED_07", 0x07, 0x1},        // (- /-) TODO: unused XR 07
    {"XR_SCANLINE", 0x08, 0x1},         // (RO  ) scanline (including offscreen >= 480)
    {"XR_FEATURES", 0x09, 0x1},         // (RO  ) update frequency of monitor mode in BCD 1/100th Hz (0x5997 = 59.97 Hz)
    {"XR_VID_HSIZE", 0x0A, 0x1},        // (RO  ) native pixel width of monitor mode (e.g. 640/848)
    {"XR_VID_VSIZE", 0x0B, 0x1},        // (RO  ) native pixel height of monitor mode (e.g. 480)
    {"XR_UNUSED_0C", 0x0C, 0x1},        // (- /-) TODO: unused XR 0C
    {"XR_UNUSED_0D", 0x0D, 0x1},        // (- /-) TODO: unused XR 0D
    {"XR_UNUSED_0E", 0x0E, 0x1},        // (- /-) TODO: unused XR 0E
    {"XR_UNUSED_0F", 0x0F, 0x1},        // (- /-) TODO: unused XR 0F

    // Playfield A Control XR Registers
    {"XR_PA_GFX_CTRL", 0x10, 0x1},         // (R /W) playfield A graphics control
    {"XR_PA_TILE_CTRL", 0x11, 0x1},        // (R /W) playfield A tile control
    {"XR_PA_DISP_ADDR", 0x12, 0x1},        // (R /W) playfield A display VRAM start address
    {"XR_PA_LINE_LEN", 0x13, 0x1},         // (R /W) playfield A display line width in words
    {"XR_PA_HV_FSCALE", 0x14, 0x1},        // (R /W) playfield A horizontal and vertical fractional scale
    {"XR_PA_HV_SCROLL", 0x15, 0x1},        // (R /W) playfield A horizontal and vertical fine scroll
    {"XR_PA_LINE_ADDR", 0x16, 0x1},        // (- /W) playfield A scanline start address (loaded at start of line)
    {"XR_PA_UNUSED_17", 0x17, 0x1},        // // TODO: colorbase?

    // Playfield B Control XR Registers
    {"XR_PB_GFX_CTRL", 0x18, 0x1},         // (R /W) playfield B graphics control
    {"XR_PB_TILE_CTRL", 0x19, 0x1},        // (R /W) playfield B tile control
    {"XR_PB_DISP_ADDR", 0x1A, 0x1},        // (R /W) playfield B display VRAM start address
    {"XR_PB_LINE_LEN", 0x1B, 0x1},         // (R /W) playfield B display line width in words
    {"XR_PB_HV_FSCALE", 0x1C, 0x1},        // (R /W) playfield B horizontal and vertical fractional scale
    {"XR_PB_HV_SCROLL", 0x1D, 0x1},        // (R /W) playfield B horizontal and vertical fine scroll
    {"XR_PB_LINE_ADDR", 0x1E, 0x1},        // (- /W) playfield B scanline start address (loaded at start of line)
    {"XR_PB_UNUSED_1F", 0x1F, 0x1},        // // TODO: colorbase?

    // Blitter Registers
    {"XR_BLIT_CTRL", 0x20, 0x1},         // (R /W) blit control (transparency control, logic op and op input flags)
    {"XR_BLIT_MOD_A", 0x21, 0x1},        // (R /W) blit line modulo added to SRC_A (XOR if A const)
    {"XR_BLIT_SRC_A", 0x22, 0x1},        // (R /W) blit A source VRAM read address / constant value
    {"XR_BLIT_MOD_B", 0x23, 0x1},        // (R /W) blit line modulo added to SRC_B (XOR if B const)
    {"XR_BLIT_SRC_B", 0x24, 0x1},        // (R /W) blit B AND source VRAM read address / constant value
    {"XR_BLIT_MOD_C", 0x25, 0x1},        // (R /W) blit line XOR modifier for C_VAL const
    {"XR_BLIT_VAL_C", 0x26, 0x1},        // (R /W) blit C XOR constant value
    {"XR_BLIT_MOD_D", 0x27, 0x1},        // (R /W) blit modulo added to D destination after each line
    {"XR_BLIT_DST_D", 0x28, 0x1},        // (R /W) blit D VRAM destination write address
    {"XR_BLIT_SHIFT", 0x29, 0x1},        // (R /W) blit first and last word nibble masks and nibble right shift (0-3)
    {"XR_BLIT_LINES", 0x2A, 0x1},        // (R /W) blit number of lines minus 1
    {"XR_BLIT_WORDS", 0x2B, 0x1},        // (R /W) blit word count minus 1 per line (write starts blit operation)
    {"XR_UNUSED_2C", 0x2C, 0x1},         // (- /-) TODO: unused XR 2C
    {"XR_UNUSED_2D", 0x2D, 0x1},         // (- /-) TODO: unused XR 2D
    {"XR_UNUSED_2E", 0x2E, 0x1},         // (- /-) TODO: unused XR 2E
    {"XR_UNUSED_2F", 0x2F, 0x1},         // (- /-) TODO: unused XR 2F

    // Audio Registers
    {"XR_AUD0_VOL", 0x30, 0x1},           // (WO) // TODO: WIP
    {"XR_AUD0_PERIOD", 0x31, 0x1},        // (WO) // TODO: WIP
    {"XR_AUD0_START", 0x32, 0x1},         // (WO) // TODO: WIP
    {"XR_AUD0_LENGTH", 0x33, 0x1},        // (WO) // TODO: WIP
    {"XR_AUD1_VOL", 0x30, 0x1},           // (WO) // TODO: WIP
    {"XR_AUD1_PERIOD", 0x31, 0x1},        // (WO) // TODO: WIP
    {"XR_AUD1_START", 0x32, 0x1},         // (WO) // TODO: WIP
    {"XR_AUD1_LENGTH", 0x33, 0x1},        // (WO) // TODO: WIP
    {"XR_AUD2_VOL", 0x30, 0x1},           // (WO) // TODO: WIP
    {"XR_AUD2_PERIOD", 0x31, 0x1},        // (WO) // TODO: WIP
    {"XR_AUD2_START", 0x32, 0x1},         // (WO) // TODO: WIP
    {"XR_AUD2_LENGTH", 0x33, 0x1},        // (WO) // TODO: WIP
    {"XR_AUD3_VOL", 0x30, 0x1},           // (WO) // TODO: WIP
    {"XR_AUD3_PERIOD", 0x31, 0x1},        // (WO) // TODO: WIP
    {"XR_AUD3_START", 0x32, 0x1},         // (WO) // TODO: WIP
    {"XR_AUD3_LENGTH", 0x33, 0x1},        // (WO) // TODO: WIP

    {"XR_TILE_ADDR", 0x4000, 0x1400},         // (R/W) 0x4000-0x53FF tile glyph/tile map memory
    {"XR_COLOR_A", 0x8000, 0x0100},           // (R/W) 0x8000-0x80FF A 256 entry color lookup memory
    {"XR_COLOR_B", 0x8100, 0x0100},           // (R/W) 0x8100-0x81FF B 256 entry color lookup memory
    {"XR_COPPER_ADDR", 0xC000, 0x800},        // (R/W) 0xC000-0xC7FF copper program memory (32-bit instructions)
    {NULL, 0, 0}};

// Xosera Main Registers (XM Registers, directly CPU accessable)
const addr_range_t xm_regs[] = {
    {"XM_SYS_CTRL", 0x00, 0x01},        // (R /W+) status bits, FPGA config, write masking
    {"XM_INT_CTRL", 0x01, 0x01},        // (R /W ) interrupt status/control
    {"XM_TIMER", 0x02, 0x01},           // (RO   ) read 1/10th millisecond timer
    {"XM_RD_XADDR", 0x03, 0x01},        // (R /W+) XR register/address for XM_XDATA read access
    {"XM_WR_XADDR", 0x04, 0x01},        // (R /W ) XR register/address for XM_XDATA write access
    {"XM_XDATA", 0x05, 0x01},           // (R /W+) read/write XR register/memory at XM_RD_XADDR/XM_WR_XADDR
    {"XM_RD_INCR", 0x06, 0x01},         // (R /W ) increment value for XM_RD_ADDR read from XM_DATA/XM_DATA_2
    {"XM_RD_ADDR", 0x07, 0x01},         // (R /W+) VRAM address for reading from VRAM when XM_DATA/XM_DATA_2 is read
    {"XM_WR_INCR", 0x08, 0x01},         // (R /W ) increment value for XM_WR_ADDR on write to XM_DATA/XM_DATA_2
    {"XM_WR_ADDR", 0x09, 0x01},         // (R /W ) VRAM address for writing to VRAM when XM_DATA/XM_DATA_2 is written
    {"XM_DATA", 0x0A, 0x01},        // (R+/W+) read/write VRAM word at XM_RD_ADDR/XM_WR_ADDR & add XM_RD_INCR/XM_WR_INCR
    {"XM_DATA_2", 0x0B, 0x01},           // (R+/W+) 2nd XM_DATA(to allow for 32-bit read/write access)
    {"XM_RW_INCR", 0x0C, 0x01},          // (R /W ) XM_RW_ADDR increment value on read/write of XM_RW_DATA/XM_RW_DATA_2
    {"XM_RW_ADDR", 0x0D, 0x01},          // (R /W+) read/write address for VRAM access from XM_RW_DATA/XM_RW_DATA_2
    {"XM_RW_DATA", 0x0E, 0x01},          // (R+/W+) read/write VRAM word at XM_RW_ADDR (and add XM_RW_INCR)
    {"XM_RW_DATA_2", 0x0F, 0x01},        // (R+/W+) 2nd XM_RW_DATA(to allow for 32-bit read/write access)
    {NULL, 0, 0}};

// NOTE: These are bits in high byte of SYS_CTRL word (fastest to access)
const addr_range_t sys_ctrl_status[] = {
    {"MEM_BUSY", 7, 1},          // (RO   )  memory read/write operation pending (with contended memory)
    {"BLIT_FULL", 6, 1},         // (RO   )  blitter queue is full, do not write new operation to blitter registers
    {"BLIT_BUSY", 5, 1},         // (RO   )  blitter is still busy performing an operation (not done)
    {"UNUSED_12", 4, 1},         // (RO   )  unused (reads 0)
    {"HBLANK", 3, 1},            // (RO   )  video signal is in horizontal blank period
    {"VBLANK", 2, 1},            // (RO   )  video signal is in vertical blank period
    {"UNUSED_9", 1, 1},          // (RO   )  unused (reads 0)
    {"RW_RD_INCR", 0, 1},        // (R / W)  increment XM_RW_ADDR after XM_RW_DATA/XM_RW_DATA_2 read
    {NULL, 0, 0}};

// XR Extended Register / Region (accessed via XM_RD_XADDR/XM_WR_XADDR and XM_XDATA)