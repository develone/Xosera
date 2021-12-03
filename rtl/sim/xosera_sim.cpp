// C++ "driver" for Xosera Verilator simulation
//
// vim: set et ts=4 sw=4
//
// Thanks to Dan "drr" Rodrigues for the amazing icestation-32 project which
// has a nice example of how to use Verilator with Yosys and SDL.  This code
// was created starting with that (so drr gets most of the credit).

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "xosera_defs.h"

#include "verilated.h"

#include "Vxosera_main.h"

#include "Vxosera_main_colormem.h"
#include "Vxosera_main_vram.h"
#include "Vxosera_main_vram_arb.h"
#include "Vxosera_main_xosera_main.h"
#include "Vxosera_main_xrmem_arb.h"

#define USE_FST 1
#if USE_FST
#include "verilated_fst_c.h"        // for VM_TRACE
#else
#include "verilated_vcd_c.h"        // for VM_TRACE
#endif
#include <SDL.h>        // for SDL_RENDER
#include <SDL_image.h>

#define LOGDIR "sim/logs/"

#define MAX_TRACE_FRAMES 8        // video frames to dump to VCD file (and then screen-shot and exit)
#define MAX_UPLOADS      8        // maximum number of "payload" uploads

// Current simulation time (64-bit unsigned)
vluint64_t main_time         = 0;
vluint64_t first_frame_start = 0;
vluint64_t frame_start_time  = 0;

volatile bool done;
bool          sim_render = SDL_RENDER;
bool          sim_bus    = BUS_INTERFACE;
bool          wait_close = false;

bool vsync_detect = false;
bool vtop_detect  = false;

int          num_uploads;
int          next_upload;
const char * upload_name[MAX_UPLOADS];
uint8_t *    upload_payload[MAX_UPLOADS];
int          upload_size[MAX_UPLOADS];
uint8_t      upload_buffer[128 * 1024];

uint16_t last_read_val;

static FILE * logfile;
static char   log_buff[16384];

static void log_printf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_buff, sizeof(log_buff), fmt, args);
    fputs(log_buff, stdout);
    fputs(log_buff, logfile);
    va_end(args);
}

static void logonly_printf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_buff, sizeof(log_buff), fmt, args);
    fputs(log_buff, logfile);
    va_end(args);
}

class BusInterface
{
    const int   BUS_START_TIME = 1000000;        // after init
    const float BUS_CLOCK_DIV  = 5;              // min 4

    enum
    {
        XM_XR_ADDR = 0x0,        // (R /W+) XR register number/address for XM_XR_DATA read/write access
        XM_XR_DATA = 0x1,        // (R /W+) read/write XR register/memory at XM_XR_ADDR (XM_XR_ADDR incr. on write)
        XM_RD_INCR = 0x2,        // (R /W ) increment value for XM_RD_ADDR read from XM_DATA/XM_DATA_2
        XM_RD_ADDR = 0x3,        // (R /W+) VRAM address for reading from VRAM when XM_DATA/XM_DATA_2 is read
        XM_WR_INCR = 0x4,        // (R /W ) increment value for XM_WR_ADDR on write to XM_DATA/XM_DATA_2
        XM_WR_ADDR = 0x5,        // (R /W ) VRAM address for writing to VRAM when XM_DATA/XM_DATA_2 is written
        XM_DATA   = 0x6,        // (R+/W+) read/write VRAM word at XM_RD_ADDR/XM_WR_ADDR (and add XM_RD_INCR/XM_WR_INCR)
        XM_DATA_2 = 0x7,        // (R+/W+) 2nd XM_DATA(to allow for 32-bit read/write access)
        XM_SYS_CTRL  = 0x8,        // (R /W+) busy status, FPGA reconfig, interrupt status/control, write masking
        XM_TIMER     = 0x9,        // (RO   ) read 1/10th millisecond timer
        XM_LFSR      = 0xA,        // (R /W ) LFSR pseudo-random register // TODO: keep this?
        XM_UNUSED_B  = 0xB,        // (R /W ) unused direct register 0xB // TODO: Use for XM_XR_DATA_2
        XM_RW_INCR   = 0xC,        // (R /W ) XM_RW_ADDR increment value on read/write of XM_RW_DATA/XM_RW_DATA_2
        XM_RW_ADDR   = 0xD,        // (R /W+) read/write address for VRAM access from XM_RW_DATA/XM_RW_DATA_2
        XM_RW_DATA   = 0xE,        // (R+/W+) read/write VRAM word at XM_RW_ADDR (and add XM_RW_INCR)
        XM_RW_DATA_2 = 0xF         // (R+/W+) 2nd XM_RW_DATA(to allow for 32-bit read/write access)
    };

    enum
    {
        // XR Register Regions
        XR_CONFIG_REGS   = 0x0000,        // 0x0000-0x000F 16 config/copper registers
        XR_PA_REGS       = 0x0010,        // 0x0000-0x0017 8 playfield A video registers
        XR_PB_REGS       = 0x0018,        // 0x0000-0x000F 8 playfield B video registers
        XR_BLIT_REGS     = 0x0030,        // 0x0000-0x000F 16 blit registers [TBD]
        XR_POLYDRAW_REGS = 0x0040,        // 0x0000-0x000F 16 line/polygon draw registers [TBD]

        // XR Memory Regions
        XR_COLOR_MEM  = 0x8000,        // 0x8000-0x81FF 2 x 256 16-bit A & B color lookup table (0xXRGB)
        XR_TILE_MEM   = 0xA000,        // 0xA000-0xB3FF 5K 16-bit words of tile/font memory
        XR_COPPER_MEM = 0xC000,        // 0xC000-0xC7FF 2K 16-bit words copper program memory
        XR_UNUSED_MEM = 0xE000,        // 0xE000-0xFFFF (currently unused)
    };

    enum
    {
        // Video Config / Copper XR Registers
        XR_VID_CTRL   = 0x00,        // (R /W) display control and border color index
        XR_COPP_CTRL  = 0x01,        // (R /W) display synchronized coprocessor control
        XR_CURSOR_X   = 0x02,        // (R /W) sprite cursor X position
        XR_CURSOR_Y   = 0x03,        // (R /W) sprite cursor Y position
        XR_VID_TOP    = 0x04,        // (R /W) top line of active display window (typically 0)
        XR_VID_BOTTOM = 0x05,        // (R /W) bottom line of active display window (typically 479)
        XR_VID_LEFT   = 0x06,        // (R /W) left edge of active display window (typically 0)
        XR_VID_RIGHT  = 0x07,        // (R /W) right edge of active display window (typically 639 or 847)
        XR_SCANLINE   = 0x08,        // (RO  ) [15] in V blank, [14] in H blank [10:0] V scanline
        XR_UNUSED_09  = 0x09,        // (RO  )
        XR_VERSION    = 0x0A,        // (RO  ) Xosera optional feature bits [15:8] and version code [7:0] [TODO]
        XR_GITHASH_H  = 0x0B,        // (RO  ) [15:0] high 16-bits of 32-bit Git hash build identifier
        XR_GITHASH_L  = 0x0C,        // (RO  ) [15:0] low 16-bits of 32-bit Git hash build identifier
        XR_VID_HSIZE  = 0x0D,        // (RO  ) native pixel width of monitor mode (e.g. 640/848)
        XR_VID_VSIZE  = 0x0E,        // (RO  ) native pixel height of monitor mode (e.g. 480)
        XR_VID_VFREQ  = 0x0F,        // (RO  ) update frequency of monitor mode in BCD 1/100th Hz (0x5997 = 59.97 Hz)

        // Playfield A Control XR Registers
        XR_PA_GFX_CTRL  = 0x10,        //  playfield A graphics control
        XR_PA_TILE_CTRL = 0x11,        //  playfield A tile control
        XR_PA_DISP_ADDR = 0x12,        //  playfield A display VRAM start address
        XR_PA_LINE_LEN  = 0x13,        //  playfield A display line width in words
        XR_PA_HV_SCROLL = 0x14,        //  playfield A horizontal and vertical fine scroll
        XR_PA_LINE_ADDR = 0x15,        //  playfield A scanline start address (loaded at start of line)
        XR_PA_UNUSED_16 = 0x16,        //
        XR_PA_UNUSED_17 = 0x17,        //

        // Playfield B Control XR Registers
        XR_PB_GFX_CTRL  = 0x18,        //  playfield B graphics control
        XR_PB_TILE_CTRL = 0x19,        //  playfield B tile control
        XR_PB_DISP_ADDR = 0x1A,        //  playfield B display VRAM start address
        XR_PB_LINE_LEN  = 0x1B,        //  playfield B display line width in words
        XR_PB_HV_SCROLL = 0x1C,        //  playfield B horizontal and vertical fine scroll
        XR_PB_LINE_ADDR = 0x1D,        //  playfield B scanline start address (loaded at start of line)
        XR_PB_UNUSED_1E = 0x1E,        //
        XR_PB_UNUSED_1F = 0x1F,        //

        XR_BLIT_CTRL  = 0x20,        // (R /W) blit control bits (logic ops, A addr/const, B addr/const, transparent)
        XR_BLIT_SHIFT = 0x21,        // (R /W) blit nibble shift (0-3)
        XR_BLIT_MOD_A = 0x22,        // (R /W) blit modulo added to A between lines (rectangular blit)
        XR_BLIT_MOD_B = 0x23,        // (R /W) blit modulo added to B between lines (rectangular blit)
        XR_BLIT_MOD_C = 0x24,        // (R /W) blit modulo added to C between lines (rectangular blit)
        XR_BLIT_MOD_D = 0x25,        // (R /W) blit modulo added to D between lines (rectangular blit)
        XR_BLIT_SRC_A = 0x26,        // (R /W) blit A source VRAM read address / constant value
        XR_BLIT_SRC_B = 0x27,        // (R /W) blit B source VRAM read address / constant value
        XR_BLIT_VAL_C = 0x28,        // (R /W) blit C source constant value
        XR_BLIT_DST_D = 0x29,        // (R /W) blit D destination write address
        XR_BLIT_LINES = 0x2A,        // (R /W) blit number of lines for rectangular blit
        XR_BLIT_WORDS = 0x2B         // (R /W) blit word count minus 1, starts operation (width when LINES > 0)
    };

    static const char * reg_name[];
    enum
    {
        BUS_START,
        BUS_HOLD,
        BUS_STROBEOFF,
        BUS_END
    };

    bool    enable;
    int64_t last_time;
    int     state;
    int     index;
    bool    wait_vsync;
    bool    wait_vtop;
    bool    wait_blit;
    bool    data_upload;
    int     data_upload_mode;
    int     data_upload_num;
    int     data_upload_count;
    int     data_upload_index;

    static int      test_data_len;
    static uint16_t test_data[16384];

public:
public:
    void set_cmdline_data(int argc, char ** argv, int & nextarg)
    {
        size_t len = 0;
        for (int i = nextarg; i < argc && len < sizeof(test_data); i++)
        {
            char * endptr = nullptr;
            int    value  = static_cast<int>(strtoul(argv[i], &endptr, 0) & 0x1fffUL);
            if (endptr != nullptr && *endptr == '\0')
            {
                test_data[len] = value;
                len++;
            }
            else
            {
                break;
            }
        }

        if (len != 0)
        {
            test_data_len = len;
        }
    }

    void init(Vxosera_main * top, bool _enable)
    {
        enable            = _enable;
        index             = 0;
        state             = BUS_START;
        wait_vsync        = false;
        wait_vtop         = false;
        wait_blit         = false;
        data_upload       = false;
        data_upload_mode  = 0;
        data_upload_num   = 0;
        data_upload_count = 0;
        data_upload_index = 0;
        top->bus_cs_n_i   = 1;
    }

    void process(Vxosera_main * top)
    {
        char tempstr[256];
        // bus test
        if (enable && main_time >= BUS_START_TIME)
        {
            if (wait_vsync)
            {
                if (vsync_detect)
                {
                    logonly_printf("[@t=%lu  ... VSYNC arrives]\n", main_time);
                    wait_vsync = false;
                }
                return;
            }

            if (wait_vtop)
            {
                if (vtop_detect)
                {
                    logonly_printf("[@t=%lu  ... VTOP arrives]\n", main_time);
                    wait_vtop = false;
                }
                return;
            }


            int64_t bus_time = (main_time - BUS_START_TIME) / BUS_CLOCK_DIV;

            if (bus_time >= last_time)
            {
                last_time = bus_time + 1;

                // REG_END
                if (!data_upload && test_data[index] == 0xffff)
                {
                    logonly_printf("[@t=%lu] REG_END hit\n", main_time);
                    done      = true;
                    enable    = false;
                    last_time = bus_time - 1;
                    return;
                }
                // REG_WAITVSYNC
                if (!data_upload && test_data[index] == 0xfffe)
                {
                    logonly_printf("[@t=%lu] Wait VSYNC...\n", main_time);
                    wait_vsync = true;
                    index++;
                    return;
                }
                // REG_WAITVTOP
                if (!data_upload && test_data[index] == 0xfffd)
                {
                    logonly_printf("[@t=%lu] Wait VTOP...\n", main_time);
                    wait_vtop = true;
                    index++;
                    return;
                }
                // REG_WAIT_BLIT_READY
                if (!data_upload && test_data[index] == 0xfffc)
                {
                    last_time = bus_time - 1;
                    if (!(last_read_val & 0x20))        // blit_full bit
                    {
                        logonly_printf("[@t=%lu] blit_full clear (SYS_CTRL.L=0x%02x)\n", main_time, last_read_val);
                        index++;
                        last_read_val = 0;
                        wait_blit     = false;
                        return;
                    }
                    else if (!wait_blit)
                    {
                        logonly_printf("[@t=%lu] Waiting until SYS_CTRL.L blit_full is clear...\n", main_time);
                    }
                    wait_blit = true;
                    index--;
                    return;
                }
                // REG_WAIT_BLIT_DONE
                if (!data_upload && test_data[index] == 0xfffb)
                {
                    last_time = bus_time - 1;
                    if (!(last_read_val & 0x40))        // blit_busy bit
                    {
                        logonly_printf("[@t=%lu] blit_busy clear (SYS_CTRL.L=0x%02x)\n", main_time, last_read_val);
                        index++;
                        last_read_val = 0;
                        wait_blit     = false;
                        return;
                    }
                    else if (!wait_blit)
                    {
                        logonly_printf("[@t=%lu] Waiting until SYS_CTRL.L blit_busy is clear...\n", main_time);
                    }
                    wait_blit = true;
                    index--;
                    return;
                }
                else if (!data_upload && (test_data[index] & 0xfffe) == 0xfff0)
                {
                    data_upload       = upload_size[data_upload_num] > 0;
                    data_upload_mode  = test_data[index] & 0x1;
                    data_upload_count = upload_size[data_upload_num];        // byte count
                    data_upload_index = 0;
                    logonly_printf("[Upload #%d started, %d bytes, mode %s]\n",
                                   data_upload_num + 1,
                                   data_upload_count,
                                   data_upload_mode ? "XR_DATA" : "VRAM_DATA");

                    index++;
                }
                int rd_wr   = (test_data[index] & 0xC000) == 0x8000 ? 1 : 0;
                int bytesel = (test_data[index] & 0x1000) ? 1 : 0;
                int reg_num = (test_data[index] >> 8) & 0xf;
                int data    = test_data[index] & 0xff;

                if (data_upload && state == BUS_START)
                {
                    bytesel = data_upload_index & 1;
                    reg_num = data_upload_mode ? XM_XR_DATA : XM_DATA;
                    data    = upload_payload[data_upload_num][data_upload_index++];
                }

                switch (state)
                {
                    case BUS_START:

                        top->bus_cs_n_i    = 1;
                        top->bus_bytesel_i = bytesel;
                        top->bus_rd_nwr_i  = rd_wr;
                        top->bus_reg_num_i = reg_num;
                        top->bus_data_i    = data;
                        if (data_upload && data_upload_index < 16)
                        {
                            logonly_printf("[@t=%lu] ", main_time);
                            sprintf(tempstr, "r[0x%x] %s.%3s", reg_num, reg_name[reg_num], bytesel ? "lsb*" : "msb");
                            logonly_printf("  %-25.25s <= %s%02x%s\n",
                                           tempstr,
                                           bytesel ? "__" : "",
                                           data & 0xff,
                                           bytesel ? "" : "__");
                            if (data_upload_index == 15)
                            {
                                logonly_printf("  ...\n");
                            }
                        }
                        break;
                    case BUS_HOLD:
                        break;
                    case BUS_STROBEOFF:
                        if (rd_wr)
                        {
                            if (!wait_blit)
                            {
                                logonly_printf("[@t=%lu] Read  Reg %s (#%02x.%s) => %s%02x%s\n",
                                               main_time,
                                               reg_name[reg_num],
                                               reg_num,
                                               bytesel ? "L" : "H",
                                               bytesel ? "__" : "",
                                               top->bus_data_o,
                                               bytesel ? "" : "__");
                            }
                            if (bytesel)
                            {
                                last_read_val = (last_read_val & 0xff00) | top->bus_data_o;
                            }
                            else
                            {
                                last_read_val = (last_read_val & 0x00ff) | (top->bus_data_o << 8);
                            }
                        }
                        else if (!data_upload)
                        {
                            logonly_printf("[@t=%lu] Write Reg %s (#%02x.%s) <= %s%02x%s\n",
                                           main_time,
                                           reg_name[reg_num],
                                           reg_num,
                                           bytesel ? "L" : "H",
                                           bytesel ? "__" : "",
                                           top->bus_data_i,
                                           bytesel ? "" : "__");
                        }
                        top->bus_cs_n_i = 0;
                        break;
                    case BUS_END:
                        top->bus_cs_n_i    = 0;
                        top->bus_bytesel_i = 0;
                        top->bus_rd_nwr_i  = 0;
                        top->bus_reg_num_i = 0;
                        top->bus_data_i    = 0;
                        //                        last_time          = bus_time + 9;
                        if (data_upload)
                        {
                            if (data_upload_index >= data_upload_count)
                            {
                                data_upload = false;
                                logonly_printf("[Upload #%d completed]\n", data_upload_num + 1);
                                data_upload_num++;
                            }
                        }
                        else if (++index >= test_data_len)
                        {
                            enable = false;
                        }
                        break;
                    default:
                        assert(false);
                }
                state = state + 1;
                if (state > BUS_END)
                {
                    state = BUS_START;
                }
            }
        }
    }
};

const char * BusInterface::reg_name[] = {"XM_XR_ADDR  ",
                                         "XM_XR_DATA  ",
                                         "XM_RD_INCR  ",
                                         "XM_RD_ADDR  ",
                                         "XM_WR_INCR  ",
                                         "XM_WR_ADDR  ",
                                         "XM_DATA     ",
                                         "XM_DATA_2   ",
                                         "XM_SYS_CTRL ",
                                         "XM_TIMER    ",
                                         "XM_LFSR     ",
                                         "XM_UNUSED_B ",
                                         "XM_RW_INCR  ",
                                         "XM_RW_ADDR  ",
                                         "XM_RW_DATA  ",
                                         "XM_RW_DATA_2"};

#define REG_B(r, v) (((BusInterface::XM_##r) | 0x10) << 8) | ((v)&0xff)
#define REG_W(r, v)                                                                                                    \
    ((BusInterface::XM_##r) << 8) | (((v) >> 8) & 0xff), (((BusInterface::XM_##r) | 0x10) << 8) | ((v)&0xff)
#define REG_RW(r)        (((BusInterface::XM_##r) | 0x80) << 8), (((BusInterface::XM_##r) | 0x90) << 8)
#define XREG_SETW(xr, v) REG_W(XR_ADDR, XR_##xr), REG_W(XR_DATA, (v))

#define REG_UPLOAD()          0xfff0
#define REG_UPLOAD_AUX()      0xfff1
#define REG_WAIT_BLIT_READY() (((BusInterface::XM_SYS_CTRL) | 0x90) << 8), 0xfffc
#define REG_WAIT_BLIT_DONE()  (((BusInterface::XM_SYS_CTRL) | 0x90) << 8), 0xfffb
#define REG_WAITVTOP()        0xfffd
#define REG_WAITVSYNC()       0xfffe
#define REG_END()             0xffff

#define X_COLS 80
#define W_4BPP (320 / 4)
#define H_4BPP (240)

#define W_LOGO (32 / 4)
#define H_LOGO (16)

BusInterface bus;
int          BusInterface::test_data_len    = 999;
uint16_t     BusInterface::test_data[16384] = {
    // test data
    REG_WAITVSYNC(),
    REG_WAITVTOP(),
    REG_WAITVSYNC(),        // show boot screen
                            //    REG_WAITVTOP(),         // show boot screen

    XREG_SETW(PA_GFX_CTRL, 0x005F),         // bitmap, 4-bpp, Hx2, Vx2
    XREG_SETW(PA_TILE_CTRL, 0x000F),        // tileset 0x0000 in TILEMEM, tilemap in VRAM, 16-high font
    XREG_SETW(PA_DISP_ADDR, 0x0000),        // display start address
    XREG_SETW(PA_LINE_LEN, 320 / 4),        // display line word length (320 pixels with 4 pixels per word at 4-bpp)

    // fill screen
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0FF0),
    XREG_SETW(BLIT_SHIFT, 0xFF00),
    XREG_SETW(BLIT_MOD_A, 0x0000),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP),
    XREG_SETW(BLIT_SRC_A, 0x5858),
    XREG_SETW(BLIT_SRC_B, 0x8888),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000),
    XREG_SETW(BLIT_LINES, H_4BPP / 2 - 1),
    XREG_SETW(BLIT_WORDS, W_4BPP - 1),
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0FF0),
    XREG_SETW(BLIT_SHIFT, 0xFF00),
    XREG_SETW(BLIT_MOD_A, 0x0000),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP),
    XREG_SETW(BLIT_SRC_A, 0x8585),
    XREG_SETW(BLIT_SRC_B, 0x8888),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, W_4BPP),
    XREG_SETW(BLIT_LINES, H_4BPP / 2 - 1),
    XREG_SETW(BLIT_WORDS, W_4BPP - 1),
    REG_WAIT_BLIT_DONE(),
    REG_WAITVSYNC(),
    REG_WAITVTOP(),

    REG_W(WR_INCR, 0x0001),        // 16x16 logo to 0xF000
    REG_W(WR_ADDR, 0xF000),
    REG_UPLOAD(),
    REG_WAITVSYNC(),

#if 0
    // 2D fill 1/4 screen
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0FF0),
    XREG_SETW(BLIT_SHIFT, 0xFF00),
    XREG_SETW(BLIT_MOD_A, 0x0000),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - (W_4BPP / 2)),
    XREG_SETW(BLIT_SRC_A, 0x0000),
    XREG_SETW(BLIT_SRC_B, 0x8888),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000),
    XREG_SETW(BLIT_LINES, H_4BPP / 2 - 1),
    XREG_SETW(BLIT_WORDS, W_4BPP / 2 - 1),
    REG_WAIT_BLIT_DONE(),
    REG_WAITVSYNC(),

    REG_W(XR_ADDR, XR_COLOR_MEM),        // upload 4-bpp color palette
    REG_UPLOAD_AUX(),

    REG_W(WR_INCR, 0x0001),        // upload 4-bpp bitmap
    REG_W(WR_ADDR, 0x8000),
    REG_UPLOAD(),

    REG_WAITVSYNC(),
    REG_WAITVTOP(),
#if 0
    // 2D fill 2/3 screen
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0FF0),
    XREG_SETW(BLIT_SHIFT, 0xFF00),
    XREG_SETW(BLIT_MOD_A, 0x0000),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - (W_4BPP / 3)),
    XREG_SETW(BLIT_SRC_A, 0x0000),
    XREG_SETW(BLIT_SRC_B, 0x8888),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000),
    XREG_SETW(BLIT_LINES, H_4BPP - (H_4BPP / 3) - 1),
    XREG_SETW(BLIT_WORDS, W_4BPP - (W_4BPP / 3) - 1),

    REG_WAIT_BLIT_DONE(),
    REG_WAITVSYNC(),
#endif
#endif
    // 2D moto blit
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0060),        // transp A_4BPP, read A, const B, B = A^B, op D=A
    XREG_SETW(BLIT_SHIFT, 0xFF00),
    XREG_SETW(BLIT_MOD_A, 0x0000),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0x000F),
    XREG_SETW(BLIT_VAL_C, 0xF000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (20 * W_4BPP) + 1),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO - 1),

    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0160),         // transp A_4BPP, read A, const B, B = A^B, op D=A
    XREG_SETW(BLIT_SHIFT, 0x7801),        // mask 1 nibble from left, and 3 nibbles from right, shift 1 nibble
    XREG_SETW(BLIT_MOD_A, -1),            // compensate for extra word width
    XREG_SETW(BLIT_MOD_B, 0x0000),        // const B term
    XREG_SETW(BLIT_MOD_C, 0x0000),        // const C term
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),               // compensate for extra word width
    XREG_SETW(BLIT_SRC_A, 0xF000),                            // A=source address
    XREG_SETW(BLIT_SRC_B, 0xFFFF),                            // B=const term (B term will also XOR'd with A)
    XREG_SETW(BLIT_VAL_C, 0x0000),                            // C=const term (not used)
    XREG_SETW(BLIT_DST_D, 0x0000 + (40 * W_4BPP) + 1),        // D=destination address
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),        // add extra word width

    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0160),
    XREG_SETW(BLIT_SHIFT, 0x3C02),
    XREG_SETW(BLIT_MOD_A, -1),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0x1111),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (60 * W_4BPP) + 1),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO - 1 + 1),

    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0120),
    XREG_SETW(BLIT_SHIFT, 0x1E03),
    XREG_SETW(BLIT_MOD_A, -1),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0x1111),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (80 * W_4BPP) + 1),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO),

    // 2D moto blit #2
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0F20),
    XREG_SETW(BLIT_SHIFT, 0xFF00),
    XREG_SETW(BLIT_MOD_A, 0x0000),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0x8888),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (20 * W_4BPP) + 10),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO - 1),

    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0020),
    XREG_SETW(BLIT_SHIFT, 0x7801),
    XREG_SETW(BLIT_MOD_A, -1),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0xFFFF),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (40 * W_4BPP) + 10),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO),

    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0021),
    XREG_SETW(BLIT_SHIFT, 0x3C02),
    XREG_SETW(BLIT_MOD_A, -1),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0x0000),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (60 * W_4BPP) + 10),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO),

    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0060),
    XREG_SETW(BLIT_SHIFT, 0x1E03),
    XREG_SETW(BLIT_MOD_A, -1),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0x0000),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (80 * W_4BPP) + 10),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO),

    // 2D moto blit #3
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0F20),
    XREG_SETW(BLIT_SHIFT, 0xFF00),
    XREG_SETW(BLIT_MOD_A, 0x0000),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0x8888),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (20 * W_4BPP) + 20),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO - 1),

    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0020),
    XREG_SETW(BLIT_SHIFT, 0x7801),
    XREG_SETW(BLIT_MOD_A, -1),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0xFFFF),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (40 * W_4BPP) + 20),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO),

    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0021),
    XREG_SETW(BLIT_SHIFT, 0x3C02),
    XREG_SETW(BLIT_MOD_A, -1),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0x0000),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (60 * W_4BPP) + 20),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO),

    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0060),
    XREG_SETW(BLIT_SHIFT, 0x1E03),
    XREG_SETW(BLIT_MOD_A, -1),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0x0000),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (80 * W_4BPP) + 20),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO),

    // 2D moto blit #4
    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0F20),
    XREG_SETW(BLIT_SHIFT, 0xFF00),
    XREG_SETW(BLIT_MOD_A, 0x0000),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0x8888),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (20 * W_4BPP) + 30),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO - 1),

    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0020),
    XREG_SETW(BLIT_SHIFT, 0x7801),
    XREG_SETW(BLIT_MOD_A, -1),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0xFFFF),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (40 * W_4BPP) + 30),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO),

    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0021),
    XREG_SETW(BLIT_SHIFT, 0x3C02),
    XREG_SETW(BLIT_MOD_A, -1),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0x0000),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (60 * W_4BPP) + 30),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO),

    REG_WAIT_BLIT_READY(),
    XREG_SETW(BLIT_CTRL, 0x0060),
    XREG_SETW(BLIT_SHIFT, 0x1E03),
    XREG_SETW(BLIT_MOD_A, -1),
    XREG_SETW(BLIT_MOD_B, 0x0000),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, W_4BPP - W_LOGO - 1),
    XREG_SETW(BLIT_SRC_A, 0xF000),
    XREG_SETW(BLIT_SRC_B, 0x0000),
    XREG_SETW(BLIT_VAL_C, 0x0000),
    XREG_SETW(BLIT_DST_D, 0x0000 + (80 * W_4BPP) + 30),
    XREG_SETW(BLIT_LINES, H_LOGO - 1),
    XREG_SETW(BLIT_WORDS, W_LOGO),


    REG_WAIT_BLIT_DONE(),
    REG_WAITVSYNC(),

    REG_END(),
#if 0
    XREG_SETW(BLIT_CTRL, 0x0003),
    XREG_SETW(BLIT_SHIFT, 0x7C01),
    XREG_SETW(BLIT_MOD_A, 0x0050 - 4),
    XREG_SETW(BLIT_MOD_B, 0x0001),
    XREG_SETW(BLIT_MOD_C, 0x0000),
    XREG_SETW(BLIT_MOD_D, 0x0050 - 4),
    XREG_SETW(BLIT_SRC_A, 0xAAAA),
    XREG_SETW(BLIT_SRC_B, 0xBBBB),
    XREG_SETW(BLIT_VAL_C, 0xCCCC),
    XREG_SETW(BLIT_DST_D, 0x0000),
    XREG_SETW(BLIT_LINES, 0x0000),
    XREG_SETW(BLIT_WORDS, 0x0000),

    XREG_SETW(PB_GFX_CTRL, 0x0000),                 // set disp in tile
    XREG_SETW(PB_TILE_CTRL),        // set 4-BPP BMAP
    REG_W(XR_DATA, 0x000F),
    XREG_SETW(PB_DISP_ADDR),        // set 4-BPP BMAP
    REG_W(XR_DATA, 0x1000),
    REG_WAITVTOP(),         // show boot screen
    REG_WAITVSYNC(),        // show boot screen

    XREG_SETW(PA_GFX_CTRL, 0x0040),                 // set disp in tile
    XREG_SETW(PA_TILE_CTRL),        // set 4-BPP BMAP
    REG_W(XR_DATA, 0x000F),
    REG_W(WR_INCR, 0x0001),
    REG_W(WR_ADDR, 0x0000),
    REG_UPLOAD(),
    REG_WAITVTOP(),         // show boot screen
    REG_WAITVSYNC(),        // show boot screen

    XREG_SETW(PA_GFX_CTRL, 0x0065),
    XREG_SETW(PA_TILE_CTRL, 0x000F),
    XREG_SETW(PA_DISP_ADDR, 0x0000),
    XREG_SETW(PA_LINE_LEN, 320 / 2),

    REG_W(XR_ADDR, XR_COLOR_MEM),        // upload color palette
    REG_UPLOAD_AUX(),

    REG_W(WR_INCR, 0x0001),
    REG_W(WR_ADDR, 0x0000),
    REG_UPLOAD(),

    XREG_SETW(PB_GFX_CTRL, 0x0065),
    XREG_SETW(PB_TILE_CTRL, 0x000F),
    XREG_SETW(PB_DISP_ADDR, 0x8000),
    XREG_SETW(PB_LINE_LEN, 320 / 2),

    XREG_SETW(COLOR_MEM + 0x100),        // upload color palette
    REG_UPLOAD_AUX(),

    REG_W(WR_INCR, 0x0001),
    REG_W(WR_ADDR, 0x8000),
    REG_UPLOAD(),

    REG_WAITVTOP(),         // show boot screen
    REG_WAITVSYNC(),        // show boot screen
#if 0

    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),        // read TILEMEM + 0x0a
    XREG_SETW(TILE_MEM),
    REG_RW(XR_DATA),        // read TILEMEM
    XREG_SETW(TILE_MEM + 0x0a),
    REG_RW(XR_DATA),                      // read TILEMEM + 0x0a
    REG_WAITVTOP(),                       // show boot screen
    REG_WAITVSYNC(),                      // show boot screen
    XREG_SETW(COPPER_MEM),        // setup copper program
#if 0
    // copperlist:
    REG_W(XR_DATA, 0x20a0, 0x0002),        //     skip  0, 160, 0b00010  ; Skip next if we've hit line 160
    REG_W(XR_DATA, 0x4014, 0x0000),        //     jmp   .gored           ; ... else, jump to set red
    REG_W(XR_DATA, 0x2140, 0x0002),        //     skip  0, 320, 0b00010  ; Skip next if we've hit line 320
    REG_W(XR_DATA, 0x400e, 0x0000),        //     jmp   .gogreen         ; ... else jump to set green
    REG_W(XR_DATA, 0xb000, 0x000f),        //     movep 0x000F, 0        ; Make background blue
    REG_W(XR_DATA, 0xb00a, 0x0007),        //     movep 0x0007, 0xA      ; Make foreground dark blue
    REG_W(XR_DATA, 0x0000, 0x0003),        //     nextf                  ; and we're done for this frame
    // .gogreen:
    REG_W(XR_DATA, 0xb000, 0x00f0),        //     movep 0x00F0, 0        ; Make background green
    REG_W(XR_DATA, 0xb00a, 0x0070),        //     movep 0x0070, 0xA       ; Make foreground dark green
    REG_W(XR_DATA, 0x4000, 0x0000),        //     jmp   copperlist       ; and restart
    // .gored:
    REG_W(XR_DATA, 0xb000, 0x0f00),        //     movep 0x0F00, 0        ; Make background red
    REG_W(XR_DATA, 0xb00a, 0x0700),        //     movep 0x0700, 0xA      ; Make foreground dark red
    REG_W(XR_DATA, 0x8002, 0x5A5A, 0x9002, 0x1F42, 0x4000, 0x0000),        //     jmp   copperlist       ; and restart
#else
    REG_W(XR_DATA, 0xb000, 0x0000),        //     movep 0x000F, 0        ; Make background blue

    REG_W(XR_DATA, 0x6010),        // copper splitscreen test
    REG_W(XR_DATA, 0x0055),

    REG_W(XR_DATA, 0xa00f, 0x0ec6),

    REG_W(XR_DATA, 0x00c8, 0x2782),

    REG_W(XR_DATA, 0xb000, 0x0f0f),        //     movep 0x000F, 0        ; Make background blue

    REG_W(XR_DATA, 0x6010, 0x0040, 0x6015, 0x3e80, 0xa00f, 0x0fff, 0x0000, 0x0003),
#endif

    XREG_SETW(COPP_CTRL),        // do copper test on bootscreen...
    REG_W(XR_DATA, 0x8000),
    REG_WAITVTOP(),
    XREG_SETW(TILE_MEM + 10),
    REG_RW(XR_DATA),
    XREG_SETW(TILE_MEM + 11),
    REG_RW(XR_DATA),
    XREG_SETW(TILE_MEM + 12),
    REG_RW(XR_DATA),
    XREG_SETW(TILE_MEM + 13),
    REG_RW(XR_DATA),
    XREG_SETW(PA_GFX_CTRL),        // set 4-BPP BMAP
    REG_W(XR_DATA, 0x0055),
    XREG_SETW(PA_LINE_LEN),        // 320/2/2 wide
    REG_W(XR_DATA, 80),
    REG_W(XR_ADDR, XR_COLOR_MEM),        // upload color palette
    REG_UPLOAD_AUX(),
    REG_W(WR_INCR, 0x0001),
    REG_W(WR_ADDR, 0x0000),
    REG_UPLOAD(),
    XREG_SETW(PA_GFX_CTRL),        // set 1-BPP BMAP
    REG_W(XR_DATA, 0x0040),
    REG_W(WR_INCR, 0x0001),
    REG_W(WR_ADDR, 16000),
    REG_UPLOAD(),
    REG_WAITVSYNC(),                     // show 1-BPP BMAP
    XREG_SETW(COPP_CTRL),        // disable copper so as not to ruin color image tests.
    REG_W(XR_DATA, 0x0000),
    XREG_SETW(VID_CTRL),
    REG_RW(XR_DATA),
    REG_W(TIMER, 0x0800),
    //    REG_WAITVSYNC(),                       // show 4-BPP BMAP
    XREG_SETW(PA_GFX_CTRL),        // set 8-BPP BMAP
    REG_W(XR_DATA, 0x0065),
    XREG_SETW(PA_LINE_LEN),        // 320/2 wide
    REG_W(XR_DATA, 160),
    REG_W(XR_ADDR, XR_COLOR_MEM),        // upload color palette
    REG_UPLOAD_AUX(),
    REG_W(WR_INCR, 0x0001),
    REG_W(WR_ADDR, 0x0000),
    REG_UPLOAD(),
    REG_WAITVSYNC(),        // show 8-BPP BMAP
    REG_W(WR_INCR, 0x0001),
    REG_W(WR_ADDR, 16000),
    REG_UPLOAD(),
    REG_WAITVSYNC(),        // show 1-BPP BMAP
    REG_END()
#endif
#endif
    REG_END(),
    // end test data
};

#if 0
uint16_t     BusInterface::test_stfont[1024] = {REG_W(WR_ADDR, 0x3),
                                          REG_W(WR_INC, 0x1),
                                          REG_W(DATA, 0x0200 | 'H'),
                                          REG_B(DATA, 'e'),
                                          REG_B(DATA, 'l'),
                                          REG_B(DATA, 'l'),
                                          REG_B(DATA, 'o'),
                                          REG_B(DATA, '!'),
                                          REG_W(WR_ADDR, 0x0),
                                          REG_W(DATA, 0x0e00 | '\x0e'),
                                          REG_W(DATA, 0x0e00 | '\x0f'),
                                          REG_W(WR_ADDR, X_COLS * 5),
                                          REG_W(DATA, 0x0200 | 'A'),
                                          REG_B(DATA, 't'),
                                          REG_B(DATA, 'a'),
                                          REG_B(DATA, 'r'),
                                          REG_B(DATA, 'i'),
                                          REG_B(DATA, ' '),
                                          REG_B(DATA, 'S'),
                                          REG_B(DATA, 'T'),
                                          REG_B(DATA, ' '),
                                          REG_B(DATA, '8'),
                                          REG_B(DATA, 'x'),
                                          REG_B(DATA, '1'),
                                          REG_B(DATA, '6'),
                                          REG_B(DATA, ' '),
                                          REG_B(DATA, 'F'),
                                          REG_B(DATA, 'o'),
                                          REG_B(DATA, 'n'),
                                          REG_B(DATA, 't'),
                                          REG_B(DATA, ' '),
                                          REG_B(DATA, 'T'),
                                          REG_B(DATA, 'e'),
                                          REG_B(DATA, 's'),
                                          REG_B(DATA, 't'),
                                          REG_B(DATA, ' '),
                                          REG_B(DATA, '\x1c'),
                                          REG_B(WR_INC, X_COLS - 1),
                                          REG_B(DATA, '\x1d'),
                                          REG_B(WR_INC, 1),
                                          REG_B(DATA, '\x1e'),
                                          REG_B(DATA, '\x1f'),
                                          REG_END()};
#endif

void ctrl_c(int s)
{
    (void)s;
    done = true;
}

// Called by $time in Verilog
double sc_time_stamp()
{
    return main_time;
}

int main(int argc, char ** argv)
{
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = ctrl_c;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    if ((logfile = fopen("sim/logs/xosera_vsim.log", "w")) == NULL)
    {
        if ((logfile = fopen("xosera_vsim.log", "w")) == NULL)
        {
            printf("can't create xosera_vsim.log (in \"sim/logs/\" or current directory)\n");
            exit(EXIT_FAILURE);
        }
    }

    double Hz = 1000000.0 / ((TOTAL_WIDTH * TOTAL_HEIGHT) * (1.0 / PIXEL_CLOCK_MHZ));
    log_printf("\nXosera simulation. Video Mode: %dx%d @%0.02fHz clock %0.03fMhz\n",
               VISIBLE_WIDTH,
               VISIBLE_HEIGHT,
               Hz,
               PIXEL_CLOCK_MHZ);

    int nextarg = 1;

    while (nextarg < argc && (argv[nextarg][0] == '-' || argv[nextarg][0] == '/'))
    {
        if (strcmp(argv[nextarg] + 1, "n") == 0)
        {
            sim_render = false;
        }
        else if (strcmp(argv[nextarg] + 1, "b") == 0)
        {
            sim_bus = true;
        }
        else if (strcmp(argv[nextarg] + 1, "w") == 0)
        {
            wait_close = true;
        }
        if (strcmp(argv[nextarg] + 1, "u") == 0)
        {
            nextarg += 1;
            if (nextarg >= argc)
            {
                printf("-u needs filename\n");
                exit(EXIT_FAILURE);
            }
            // upload_data = true;
            upload_name[num_uploads] = argv[nextarg];
            num_uploads++;
        }
        nextarg += 1;
    }

    if (num_uploads)
    {
        for (int u = 0; u < num_uploads; u++)
        {
            logonly_printf("Reading upload data #%d: \"%s\"...", u + 1, upload_name[u]);
            FILE * bfp = fopen(upload_name[u], "r");
            if (bfp != nullptr)
            {
                int read_size = fread(upload_buffer, 1, sizeof(upload_buffer), bfp);
                fclose(bfp);

                if (read_size > 0)
                {
                    logonly_printf("read %d bytes.\n", read_size);
                    upload_size[u]    = read_size;
                    upload_payload[u] = (uint8_t *)malloc(read_size);
                    memcpy(upload_payload[u], upload_buffer, read_size);
                }
                else
                {
                    fprintf(stderr, "Reading upload data \"%s\" error ", upload_name[u]);
                    perror("fread failed");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                fprintf(stderr, "Reading upload data \"%s\" error ", upload_name[u]);
                perror("fopen failed");
                exit(EXIT_FAILURE);
            }
        }
    }


#if BUS_INTERFACE
    // bus test data init
    bus.set_cmdline_data(argc, argv, nextarg);
#endif

    Verilated::commandArgs(argc, argv);

#if VM_TRACE
    Verilated::traceEverOn(true);
#endif

    Vxosera_main * top = new Vxosera_main;

#if SDL_RENDER
    SDL_Renderer * renderer = nullptr;
    SDL_Window *   window   = nullptr;
    if (sim_render)
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            fprintf(stderr, "SDL_Init() failed: %s\n", SDL_GetError());
            return EXIT_FAILURE;
        }
        if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0)
        {
            fprintf(stderr, "IMG_Init() failed: %s\n", SDL_GetError());
            return EXIT_FAILURE;
        }

        window = SDL_CreateWindow(
            "Xosera-sim", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, TOTAL_WIDTH, TOTAL_HEIGHT, SDL_WINDOW_SHOWN);

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        SDL_RenderSetScale(renderer, 1, 1);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
    }

    bool shot_all  = true;        // screenshot all frames
    bool take_shot = false;

#endif        // SDL_RENDER
    int  current_x          = 0;
    int  current_y          = 0;
    bool vga_hsync_previous = !H_SYNC_POLARITY;
    bool vga_vsync_previous = !V_SYNC_POLARITY;
    int  frame_num          = 0;
    int  x_max              = 0;
    int  y_max              = 0;
    int  hsync_count = 0, hsync_min = 0, hsync_max = 0;
    int  vsync_count  = 0;
    bool image_loaded = false;

#if VM_TRACE
#if USE_FST
    const auto trace_path = LOGDIR "xosera_vsim.fst";
    logonly_printf("Writing FST waveform file to \"%s\"...\n", trace_path);
    VerilatedFstC * tfp = new VerilatedFstC;
#else
    const auto trace_path = LOGDIR "xosera_vsim.vcd";
    logonly_printf("Writing VCD waveform file to \"%s\"...\n", trace_path);
    VerilatedVcdC * tfp = new VerilatedVcdC;
#endif

    top->trace(tfp, 99);        // trace to heirarchal depth of 99
    tfp->open(trace_path);
#endif

    top->reset_i = 1;        // start in reset

    bus.init(top, sim_bus);

    while (!done && !Verilated::gotFinish())
    {
        if (main_time == 4)
        {
            top->reset_i = 0;        // tale out of reset after 2 cycles
        }

#if BUS_INTERFACE
        bus.process(top);
#endif

        top->clk = 1;        // clock rising
        top->eval();

#if VM_TRACE
        if (frame_num <= MAX_TRACE_FRAMES)
            tfp->dump(main_time);
#endif
        main_time++;

        top->clk = 0;        // clock falling
        top->eval();

#if VM_TRACE
        if (frame_num <= MAX_TRACE_FRAMES)
            tfp->dump(main_time);
#endif
        main_time++;

        if (top->reconfig_o)
        {
            log_printf("FPGA RECONFIG: config #0x%x\n", top->boot_select_o);
            done = true;
        }

        if (top->bus_intr_o)
        {
            logonly_printf("[@t=%lu FPGA INTERRUPT]\n", main_time);
        }

        if (frame_num > 1)
        {
#if 0
            if (top->xosera_main->vram_arb->regs_ack_o)
            {
                if (top->xosera_main->vram_arb->regs_wr_i)
                {
                    logonly_printf(" => regs write VRAM[0x%04x]<=0x%04x\n",
                                   top->xosera_main->vram_arb->regs_addr_i,
                                   top->xosera_main->vram_arb->regs_data_i);
                }
                else
                {
                    logonly_printf(" <= regs read VRAM[0x%04x]=>0x%04x\n",
                                   top->xosera_main->vram_arb->regs_addr_i,
                                   top->xosera_main->vram_arb->vram_data_o);
                }
            }

            if (top->xosera_main->xrmem_arb->xr_ack_o)
            {
                if (top->xosera_main->xrmem_arb->xr_wr_i)
                {
                    logonly_printf(" => regs write XR[0x%04x]<=0x%04x\n",
                                   top->xosera_main->xrmem_arb->xr_addr_i,
                                   top->xosera_main->xrmem_arb->xr_data_i);
                }
                else
                {
                    logonly_printf(" <= regs read XR[0x%04x]=>0x%04x\n",
                                   top->xosera_main->xrmem_arb->xr_addr_i,
                                   top->xosera_main->xrmem_arb->xr_data_o);
                }
            }

            if (top->xosera_main->xrmem_arb->copp_xr_sel_i)
            {
                logonly_printf(" => COPPER XR write XR[0x%04x]<=0x%04x\n",
                               top->xosera_main->xrmem_arb->copp_xr_addr_i,
                               top->xosera_main->xrmem_arb->copp_xr_data_i);
            }
#endif
        }

        bool hsync = H_SYNC_POLARITY ? top->hsync_o : !top->hsync_o;
        bool vsync = V_SYNC_POLARITY ? top->vsync_o : !top->vsync_o;

#if SDL_RENDER
        if (sim_render)
        {
            if (top->dv_de_o)
            {
                // sim_render current VGA output pixel (4 bits per gun)
                SDL_SetRenderDrawColor(renderer,
                                       (top->red_o << 4) | top->red_o,
                                       (top->green_o << 4) | top->green_o,
                                       (top->blue_o << 4) | top->blue_o,
                                       255);
            }
            else
            {
                if (top->red_o != 0 || top->green_o != 0 || top->blue_o != 0)
                {
                    log_printf("Frame %3u pixel %d, %d RGB is 0x%02x 0x%02x 0x%02x when NOT visible\n",
                               frame_num,
                               current_x,
                               current_y,
                               top->red_o,
                               top->green_o,
                               top->blue_o);
                }

                // sim_render dithered border area
                if (((current_x ^ current_y) & 1) == 1)        // non-visible
                {
                    // dither with dimmed color 0
                    auto       vmem    = top->xosera_main->xrmem_arb->colormem->bram;
                    uint16_t * color0p = &vmem[0];
                    uint16_t   color0  = *color0p;
                    SDL_SetRenderDrawColor(
                        renderer, ((color0 & 0x0f00) >> 5), ((color0 & 0x00f0) >> 1), ((color0 & 0x000f) << 7), 255);
                }
                else
                {
                    SDL_SetRenderDrawColor(renderer, 0x21, vsync ? 0x41 : 0x21, hsync ? 0x41 : 0x21, 0xff);
                }
            }

            if (frame_num > 0)
            {
                SDL_RenderDrawPoint(renderer, current_x, current_y);
            }
        }
#endif
        current_x++;

        if (hsync)
            hsync_count++;

        vtop_detect = top->xosera_main->dv_de_o;

        // end of hsync
        if (!hsync && vga_hsync_previous)
        {
            if (hsync_count > hsync_max)
                hsync_max = hsync_count;
            if (hsync_count < hsync_min || !hsync_min)
                hsync_min = hsync_count;
            hsync_count = 0;

            if (current_x > x_max)
                x_max = current_x;

            current_x = 0;
            current_y++;

            if (vsync)
                vsync_count++;
        }

        vga_hsync_previous = hsync;

        vsync_detect = false;

        if (!vsync && vga_vsync_previous)
        {
            vsync_detect = true;
            if (current_y - 1 > y_max)
                y_max = current_y - 1;

            if (frame_num > 0)
            {
                if (frame_num == 1)
                {
                    first_frame_start = main_time;
                }
                vluint64_t frame_time = (main_time - frame_start_time) / 2;
                logonly_printf(
                    "[@t=%lu] Frame %3d, %lu pixel-clocks (% 0.03f msec real-time), %dx%d hsync %d, vsync %d\n",
                    main_time,
                    frame_num,
                    frame_time,
                    ((1.0 / PIXEL_CLOCK_MHZ) * frame_time) / 1000.0,
                    x_max,
                    y_max + 1,
                    hsync_max,
                    vsync_count);
#if SDL_RENDER

                if (sim_render)
                {
                    if (shot_all || take_shot || frame_num == MAX_TRACE_FRAMES)
                    {
                        int  w = 0, h = 0;
                        char save_name[256] = {0};
                        SDL_GetRendererOutputSize(renderer, &w, &h);
                        SDL_Surface * screen_shot =
                            SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
                        SDL_RenderReadPixels(
                            renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screen_shot->pixels, screen_shot->pitch);
                        sprintf(
                            save_name, LOGDIR "xosera_vsim_%dx%d_f%02d.png", VISIBLE_WIDTH, VISIBLE_HEIGHT, frame_num);
                        IMG_SavePNG(screen_shot, save_name);
                        SDL_FreeSurface(screen_shot);
                        float fnum = ((1.0 / PIXEL_CLOCK_MHZ) * ((main_time - first_frame_start) / 2)) / 1000.0;
                        log_printf("[@t=%lu] %8.03f ms frame #%3u saved as \"%s\" (%dx%d)\n",
                                   main_time,
                                   fnum,
                                   frame_num,
                                   save_name,
                                   w,
                                   h);
                        take_shot = false;
                    }

                    SDL_RenderPresent(renderer);
                    SDL_SetRenderDrawColor(renderer, 0x20, 0x20, 0x20, 0xff);
                    SDL_RenderClear(renderer);
                }
#endif
            }
            frame_start_time = main_time;
            hsync_min        = 0;
            hsync_max        = 0;
            vsync_count      = 0;
            current_y        = 0;

            if (frame_num == MAX_TRACE_FRAMES)
            {
                break;
            }

            if (TOTAL_HEIGHT == y_max + 1)
            {
                frame_num += 1;
            }
            else if (TOTAL_HEIGHT <= y_max)
            {
                log_printf("line %d >= TOTAL_HEIGHT\n", y_max);
            }
        }

        vga_vsync_previous = vsync;

#if SDL_RENDER
        if (sim_render)
        {
            SDL_Event e;
            SDL_PollEvent(&e);

            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
            {
                log_printf("Window closed\n");
                break;
            }
        }
#endif
    }

    FILE * mfp = fopen(LOGDIR "xosera_vsim_text.txt", "w");
    if (mfp != nullptr)
    {
        auto       vmem = top->xosera_main->vram_arb->vram->memory;
        uint16_t * mem  = &vmem[0];

        for (int y = 0; y < VISIBLE_HEIGHT / 16; y++)
        {
            fprintf(mfp, "%04x: ", y * (VISIBLE_WIDTH / 8));
            for (int x = 0; x < VISIBLE_WIDTH / 8; x++)
            {
                auto m      = mem[y * (VISIBLE_WIDTH / 8) + x];
                char str[4] = {0};
                if (isprint(m & 0xff))
                {
                    sprintf(str, "'%c", m & 0xff);
                }
                else
                {
                    sprintf(str, "%02x", m & 0xff);
                }

                fprintf(mfp, "%02x%s ", m >> 8, str);
            }
            fprintf(mfp, "\n");
        }
        fclose(mfp);
    }

    {
        FILE * bfp = fopen(LOGDIR "xosera_vsim_vram.bin", "w");
        if (bfp != nullptr)
        {
            auto       vmem = top->xosera_main->vram_arb->vram->memory;
            uint16_t * mem  = &vmem[0];
            fwrite(mem, 128 * 1024, 1, bfp);
            fclose(bfp);
        }
    }

    {
        FILE * tfp = fopen(LOGDIR "xosera_vsim_vram_hex.txt", "w");
        if (tfp != nullptr)
        {
            auto       vmem = top->xosera_main->vram_arb->vram->memory;
            uint16_t * mem  = &vmem[0];
            for (int i = 0; i < 65536; i += 16)
            {
                fprintf(tfp, "%04x:", i);
                for (int j = i; j < i + 16; j++)
                {
                    fprintf(tfp, " %04x", mem[j]);
                }
                fprintf(tfp, "\n");
            }
            fclose(tfp);
        }
    }


    top->final();

#if VM_TRACE
    tfp->close();
#endif

#if SDL_RENDER
    if (sim_render)
    {
        if (!wait_close)
        {
            SDL_Delay(1000);
        }
        else
        {
            fprintf(stderr, "Press RETURN:\n");
            fgetc(stdin);
        }

        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
    }
#endif

    log_printf("Simulation ended after %d frames, %lu pixel clock ticks (%.04f milliseconds)\n",
               frame_num,
               (main_time / 2),
               ((1.0 / (PIXEL_CLOCK_MHZ * 1000000)) * (main_time / 2)) * 1000.0);

    return EXIT_SUCCESS;
}
