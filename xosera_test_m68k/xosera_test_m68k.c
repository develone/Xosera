/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 * ------------------------------------------------------------
 * Copyright (c) 2021 Xark
 * MIT License
 *
 * Test and tech-demo for Xosera FPGA "graphics card"
 * ------------------------------------------------------------
 */

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>
#include <machine.h>
#include <sdfat.h>

//#define DELAY_TIME 15000        // slow human speed
//#define DELAY_TIME 5000        // human speed
#define DELAY_TIME 1000        // impatient human speed
//#define DELAY_TIME 500        // machine speed

#define COPPER_TEST    1
#define LR_MARGIN_TEST 0

#if !defined(NUM_ELEMENTS)
#define NUM_ELEMENTS(a) (sizeof(a) / sizeof(a[0]))
#endif

#include "xosera_m68k_api.h"

extern void install_intr(void);
extern void remove_intr(void);

extern volatile uint32_t XFrameCount;

bool use_sd;

// Xosera default color palette
uint16_t def_colors[256] = {
    0x0000, 0x000a, 0x00a0, 0x00aa, 0x0a00, 0x0a0a, 0x0aa0, 0x0aaa, 0x0555, 0x055f, 0x05f5, 0x05ff, 0x0f55, 0x0f5f,
    0x0ff5, 0x0fff, 0x0213, 0x0435, 0x0546, 0x0768, 0x098a, 0x0bac, 0x0dce, 0x0313, 0x0425, 0x0636, 0x0858, 0x0a7a,
    0x0c8c, 0x0eae, 0x0413, 0x0524, 0x0635, 0x0746, 0x0857, 0x0a68, 0x0b79, 0x0500, 0x0801, 0x0a33, 0x0d55, 0x0f78,
    0x0fab, 0x0fde, 0x0534, 0x0756, 0x0867, 0x0a89, 0x0b9a, 0x0dbc, 0x0ecd, 0x0200, 0x0311, 0x0533, 0x0744, 0x0966,
    0x0b88, 0x0daa, 0x0421, 0x0532, 0x0643, 0x0754, 0x0864, 0x0a75, 0x0b86, 0x0310, 0x0630, 0x0850, 0x0a70, 0x0da3,
    0x0fd5, 0x0ff7, 0x0210, 0x0432, 0x0654, 0x0876, 0x0a98, 0x0cba, 0x0edc, 0x0321, 0x0431, 0x0541, 0x0763, 0x0985,
    0x0ba7, 0x0dc9, 0x0331, 0x0441, 0x0551, 0x0662, 0x0773, 0x0884, 0x0995, 0x0030, 0x0250, 0x0470, 0x06a0, 0x08c0,
    0x0bf3, 0x0ef5, 0x0442, 0x0664, 0x0775, 0x0997, 0x0aa8, 0x0cca, 0x0ddb, 0x0010, 0x0231, 0x0341, 0x0562, 0x0673,
    0x0895, 0x0ab7, 0x0130, 0x0241, 0x0351, 0x0462, 0x0573, 0x0694, 0x07a5, 0x0040, 0x0060, 0x0180, 0x03b2, 0x05e5,
    0x08f7, 0x0af9, 0x0120, 0x0342, 0x0453, 0x0675, 0x0897, 0x0ab9, 0x0dec, 0x0020, 0x0141, 0x0363, 0x0474, 0x0696,
    0x08b8, 0x0ad9, 0x0031, 0x0142, 0x0253, 0x0364, 0x0486, 0x0597, 0x06a8, 0x0033, 0x0054, 0x0077, 0x02a9, 0x04cc,
    0x07ff, 0x09ff, 0x0354, 0x0465, 0x0576, 0x0798, 0x08a9, 0x0acb, 0x0ced, 0x0011, 0x0022, 0x0244, 0x0366, 0x0588,
    0x0699, 0x08bb, 0x0035, 0x0146, 0x0257, 0x0368, 0x0479, 0x058a, 0x069b, 0x0018, 0x003b, 0x035d, 0x047f, 0x07af,
    0x09ce, 0x0cff, 0x0123, 0x0234, 0x0456, 0x0678, 0x089a, 0x0abc, 0x0cde, 0x0013, 0x0236, 0x0347, 0x0569, 0x078b,
    0x09ad, 0x0bcf, 0x0226, 0x0337, 0x0448, 0x0559, 0x066a, 0x077c, 0x088d, 0x0209, 0x041c, 0x063f, 0x085f, 0x0b7f,
    0x0eaf, 0x0fdf, 0x0446, 0x0557, 0x0779, 0x088a, 0x0aac, 0x0bbd, 0x0ddf, 0x0103, 0x0215, 0x0437, 0x0548, 0x076a,
    0x098d, 0x0baf, 0x0315, 0x0426, 0x0537, 0x0648, 0x085a, 0x096b, 0x0a7c, 0x0405, 0x0708, 0x092a, 0x0c4d, 0x0f6f,
    0x0f9f, 0x0fbf, 0x0000, 0x0111, 0x0222, 0x0333, 0x0444, 0x0555, 0x0666, 0x0777, 0x0888, 0x0999, 0x0aaa, 0x0bbb,
    0x0ccc, 0x0ddd, 0x0eee, 0x0fff};

#if COPPER_TEST
// Copper list
const uint32_t copper_list[] = {
    // change color 0 every 30 lines
    COP_WAIT_V(30 * 0),
    // color dot test
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),
    COP_MOVEP(0xfff, 0),
    COP_MOVEP(0x000, 0),

    COP_WAIT_V(30 * 1),
    COP_MOVEP(0x111, 0),
    COP_WAIT_V(30 * 2),
    COP_MOVEP(0x222, 0),
    COP_WAIT_V(30 * 3),
    COP_MOVEP(0x333, 0),
    COP_WAIT_V(30 * 4),
    COP_MOVEP(0x444, 0),
    COP_WAIT_V(30 * 5),
    COP_MOVEP(0x555, 0),
    COP_WAIT_V(30 * 6),
    COP_MOVEP(0x666, 0),
    COP_WAIT_V(30 * 7),
    COP_MOVEP(0x777, 0),
    COP_WAIT_V(30 * 8),
    COP_MOVEP(0x888, 0),
    COP_WAIT_V(30 * 9),
    COP_MOVEP(0x999, 0),
    COP_WAIT_V(30 * 10),
    COP_MOVEP(0xaaa, 0),
    COP_WAIT_V(30 * 11),
    COP_MOVEP(0xbbb, 0),
    COP_WAIT_V(30 * 12),
    COP_MOVEP(0xccc, 0),
    COP_WAIT_V(30 * 13),
    COP_MOVEP(0xddd, 0),
    COP_WAIT_V(30 * 14),
    COP_MOVEP(0xeee, 0),
    COP_WAIT_V(30 * 15),
    COP_MOVEP(0xfff, 0),
    COP_WAIT_V(30 * 16),
    COP_END()};

const uint16_t copper_list_len = NUM_ELEMENTS(copper_list);

static_assert(NUM_ELEMENTS(copper_list) < 1024, "copper list too long");

// 320x200 copper
// Copper list
uint32_t copper_320x200[] = {
    COP_WAIT_V(40),                        // wait  0, 40                   ; Wait for line 40, H position ignored
    COP_MOVER(0x0065, PA_GFX_CTRL),        // mover 0x0065, PA_GFX_CTRL     ; Set to 8-bpp + Hx2 + Vx2
    COP_MOVER(0x0065, PB_GFX_CTRL),        // mover 0x0065, PA_GFX_CTRL     ; Set to 8-bpp + Hx2 + Vx2
    COP_WAIT_V(440),                       // wait  0, 440                  ; Wait for line 440, H position ignored
    COP_MOVER(0x00E5, PA_GFX_CTRL),        // mover 0x00E5, PA_GFX_CTRL     ; Set to Blank + 8-bpp + Hx2 + Vx2
    COP_MOVER(0x00E5, PB_GFX_CTRL),        // mover 0x00E5, PA_GFX_CTRL     ; Set to Blank + 8-bpp + Hx2 + Vx2
    COP_END()                              // nextf
};

#endif

// dummy global variable
uint32_t global;        // this is used to prevent the compiler from optimizing out tests

uint32_t mem_buffer[128 * 1024];

// timer helpers
static uint32_t start_tick;

void timer_start()
{
    uint32_t ts = XFrameCount;
    uint32_t t;
    // this waits for a "fresh tick" to reduce timing jitter
    while ((t = XFrameCount) == ts)
        ;
    start_tick = t;
}

uint32_t timer_stop()
{
    uint32_t stop_tick = XFrameCount;

    return ((stop_tick - start_tick) * 1667) / 100;
}

#if !defined(checkchar)        // newer rosco_m68k library addition, this is in case not present
bool checkchar()
{
    int rc;
    __asm__ __volatile__(
        "move.l #6,%%d1\n"        // CHECKCHAR
        "trap   #14\n"
        "move.b %%d0,%[rc]\n"
        "ext.w  %[rc]\n"
        "ext.l  %[rc]\n"
        : [rc] "=d"(rc)
        :
        : "d0", "d1");
    return rc != 0;
}
#endif

_NOINLINE bool delay_check(int ms)
{
    while (ms--)
    {
        if (checkchar())
        {
            return true;
        }

        uint16_t tms = 10;
        do
        {
            uint8_t tvb = xm_getbl(TIMER);
            while (tvb == xm_getbl(TIMER))
                ;
        } while (--tms);
    }

    return false;
}

static void dputc(char c)
{
#ifndef __INTELLISENSE__
    __asm__ __volatile__(
        "move.w %[chr],%%d0\n"
        "move.l #2,%%d1\n"        // SENDCHAR
        "trap   #14\n"
        :
        : [chr] "d"(c)
        : "d0", "d1");
#endif
}

static void dprint(const char * str)
{
    register char c;
    while ((c = *str++) != '\0')
    {
        if (c == '\n')
        {
            dputc('\r');
        }
        dputc(c);
    }
}

static char dprint_buff[4096];
static void dprintf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(dprint_buff, sizeof(dprint_buff), fmt, args);
    dprint(dprint_buff);
    va_end(args);
}

uint16_t screen_addr;
uint8_t  text_columns;
uint8_t  text_rows;
int8_t   text_h;
int8_t   text_v;
uint8_t  text_color = 0x02;        // dark green on black

static void get_textmode_settings()
{
    uint16_t vx          = (xreg_getw(PA_GFX_CTRL) & 3) + 1;
    uint16_t tile_height = (xreg_getw(PA_TILE_CTRL) & 0xf) + 1;
    screen_addr          = xreg_getw(PA_DISP_ADDR);
    text_columns         = (uint8_t)xreg_getw(PA_LINE_LEN);
    text_rows            = (uint8_t)(((xreg_getw(VID_VSIZE) / vx) + (tile_height - 1)) / tile_height);
}

static void xcls()
{
    get_textmode_settings();
    xm_setw(WR_INCR, 1);
    xm_setw(WR_ADDR, screen_addr);
    xm_setbh(DATA, text_color);
    for (uint16_t i = 0; i < (text_columns * text_rows); i++)
    {
        xm_setbl(DATA, ' ');
    }
    xm_setw(WR_ADDR, screen_addr);
}

static const char * xmsg(int x, int y, int color, const char * msg)
{
    xm_setw(WR_ADDR, (y * text_columns) + x);
    xm_setbh(DATA, color);
    char c;
    while ((c = *msg) != '\0')
    {
        msg++;
        if (c == '\n')
        {
            break;
        }

        xm_setbl(DATA, c);
    }
    return msg;
}

void wait_vsync()
{
    while (xreg_getw(SCANLINE) >= 0x8000)
        ;
    while (xreg_getw(SCANLINE) < 0x8000)
        ;
}

static inline void check_vsync()
{
    while (xreg_getw(SCANLINE) < 0x8000)
        ;
    while ((xreg_getw(SCANLINE) & 0x3ff) > 520)
        ;
}

static void install_copper()
{
    dprintf("Loading copper list...");

    xm_setw(XR_ADDR, XR_COPPER_MEM);

    for (uint16_t i = 0; i < copper_list_len; i++)
    {
        xm_setw(XR_DATA, copper_list[i] >> 16);
        xm_setw(XR_DATA, copper_list[i] & 0xffff);
    }

    dprintf("okay\n");
}

_NOINLINE void restore_colors()
{
    wait_vsync();
    xm_setw(XR_ADDR, XR_COLOR_MEM);
    uint16_t * cp = def_colors;
    for (uint16_t i = 0; i < 256; i++)
    {
        xm_setw(XR_DATA, *cp++);
    };
}

_NOINLINE void restore_colors2(uint8_t alpha)
{
    wait_vsync();
    xm_setw(XR_ADDR, XR_COLOR_MEM + 0x100);
    uint16_t * cp = def_colors;
    for (uint16_t i = 0; i < 256; i++)
    {
        uint16_t w = *cp++;
        if (i)
        {
            w = ((alpha & 0xf) << 12) | (w & 0xfff);
        }
        else
        {
            w = 0;
        }
        xm_setw(XR_DATA, w);
    };
}

// sets test blend palette
_NOINLINE void restore_colors3()
{
    wait_vsync();
    xm_setw(XR_ADDR, XR_COLOR_MEM + 0x100);
    uint16_t * cp = def_colors;
    for (uint16_t i = 0; i < 256; i++)
    {
        uint16_t w = *cp++;
        if (i)
        {
            w = ((i & 0x3) << 14) | (w & 0xfff);
        }
        else
        {
            w = 0;
        }
        xm_setw(XR_DATA, w);
    };
}

_NOINLINE void dupe_colors(int alpha)
{
    wait_vsync();
    uint16_t a = (alpha & 0xf) << 12;
    xm_setw(XR_ADDR, XR_COLOR_MEM);
    for (uint16_t i = 0; i < 256; i++)
    {
        xm_setw(XR_ADDR, XR_COLOR_MEM + i);
        while (xm_getbl(SYS_CTRL) & 0x40)
            ;

        uint16_t v = (xm_getw(XR_DATA) & 0xfff) | a;

        xm_setw(XR_ADDR, XR_COLOR_MEM + 0x100 + i);
        while (xm_getbl(SYS_CTRL) & 0x40)
            ;
        xm_setw(XR_DATA, v);
    };
}

static void load_sd_bitmap(const char * filename, int vaddr)
{
    dprintf("Loading bitmap: \"%s\"", filename);
    void * file = fl_fopen(filename, "r");

    if (file != NULL)
    {
        int cnt = 0;

        while ((cnt = fl_fread(mem_buffer, 1, 512, file)) > 0)
        {
            if ((vaddr & 0xFFF) == 0)
            {
                dprintf(".");
            }

            uint16_t * maddr = (uint16_t *)mem_buffer;
            xm_setw(WR_INCR, 1);
            xm_setw(WR_ADDR, vaddr);
            for (int i = 0; i < (cnt >> 1); i++)
            {
                xm_setw(DATA, *maddr++);
            }
            vaddr += (cnt >> 1);
        }

        fl_fclose(file);
        dprintf("done!\n");
    }
    else
    {
        dprintf(" - FAILED\n");
    }
}

static void load_sd_colors(const char * filename)
{
    dprintf("Loading colormap: \"%s\"", filename);
    void * file = fl_fopen(filename, "r");

    if (file != NULL)
    {
        int cnt   = 0;
        int vaddr = 0;

        while ((cnt = fl_fread(mem_buffer, 1, 512, file)) > 0)
        {
            if ((vaddr & 0x7) == 0)
            {
                dprintf(".");
            }

            uint16_t * maddr = (uint16_t *)mem_buffer;
            wait_vsync();
            xm_setw(XR_ADDR, XR_COLOR_MEM);
            for (int i = 0; i < (cnt >> 1); i++)
            {
                uint16_t v = *maddr++;
                xm_setw(XR_DATA, v);
            }
            vaddr += (cnt >> 1);
        }

        fl_fclose(file);
        dprintf("done!\n");
    }
    else
    {
        dprintf(" - FAILED\n");
    }
}

#define DRAW_WIDTH  ((uint16_t)320)
#define DRAW_HEIGHT ((uint16_t)240)
#define DRAW_WORDS  ((uint16_t)DRAW_WIDTH / 2)

void draw8bpp_h_line(unsigned int base, uint8_t color, int x, int y, int len)
{
    if (len < 1)
    {
        return;
    }
    uint16_t addr = base + (uint16_t)(y * DRAW_WORDS) + (uint16_t)(x >> 1);
    uint16_t word = (color << 8) | color;
    xm_setw(WR_INCR, 1);           // set write inc
    xm_setw(WR_ADDR, addr);        // set write address
    if (x & 1)
    {
        xm_setbl(SYS_CTRL, 0x3);
        xm_setw(DATA, word);        // set left edge word
        len -= 1;
        xm_setbl(SYS_CTRL, 0xf);
    }
    while (len >= 2)
    {
        xm_setw(DATA, word);        // set full word
        len -= 2;
    }
    if (len)
    {
        xm_setbl(SYS_CTRL, 0xc);
        xm_setw(DATA, word);        // set right edge word
        xm_setbl(SYS_CTRL, 0xf);
    }
}

void draw8bpp_v_line(uint16_t base, uint8_t color, int x, int y, int len)
{
    if (len < 1)
    {
        return;
    }
    uint16_t addr = base + (uint16_t)(y * DRAW_WORDS) + (uint16_t)(x >> 1);
    uint16_t word = (color << 8) | color;
    xm_setw(WR_INCR, DRAW_WORDS);        // set write inc
    xm_setw(WR_ADDR, addr);              // set write address
    if (x & 1)
    {
        xm_setbl(SYS_CTRL, 0x3);
    }
    else
    {
        xm_setbl(SYS_CTRL, 0xc);
    }
    while (len--)
    {
        xm_setw(DATA, word);        // set full word
    }
    xm_setbl(SYS_CTRL, 0xf);
}

static inline void wait_blit()
{
    while (xm_getbl(SYS_CTRL) & 0x80)
        ;
}

void test_blit()
{
    dprintf("test_blit\n");

    do
    {
        wait_vsync();
        xreg_setw(PA_GFX_CTRL, 0x0055);        // bitmap + 4-bpp + Hx2 + Vx2
        xreg_setw(PA_LINE_LEN, 80);
        xreg_setw(PA_DISP_ADDR, 0x4B00);

        xreg_setw(PB_GFX_CTRL, 0x0080);        // bitmap + 4-bpp + Hx2 + Vx2

        load_sd_colors("/pacbox-320x240_pal.raw");
        dupe_colors(0x8);
        load_sd_bitmap("/pacbox-320x240.raw", 0x0000);
        if (delay_check(DELAY_TIME))
        {
            break;
        }

        dprintf("blit from 0x0000 to 0x4B00, 0x%04X bytes\n", 320 * 240 / 4);
        xreg_setw(BLIT_RD_ADDR, 0x0000);
        xreg_setw(BLIT_WR_ADDR, 0x4B00);
        xreg_setw(BLIT_COUNT, (320 * 240 / 4) - 1);
        wait_blit();

        if (delay_check(DELAY_TIME))
        {
            break;
        }

        dprintf("blit from 0x4B00 to 0x4B01, 0x%04X bytes (clear)\n", 320 * 240 / 4);
        xreg_setw(BLIT_RD_ADDR, 0x4B00);
        xreg_setw(BLIT_WR_ADDR, 0x4B01);
        xreg_setw(BLIT_COUNT, (320 * 240 / 4) - 2);
        wait_blit();

        if (delay_check(DELAY_TIME))
        {
            break;
        }

        dprintf("blit from 0x0000 to 0x4B00, 0x%04X bytes\n", 320 * 240 / 4);
        xreg_setw(BLIT_RD_ADDR, 0x0000);
        xreg_setw(BLIT_WR_ADDR, 0x4B00);
        xreg_setw(BLIT_COUNT, (320 * 240 / 4) - 2);
        wait_blit();

        if (delay_check(DELAY_TIME))
        {
            break;
        }

        xm_setw(XR_ADDR, XR_COLOR_MEM + 15);        // set write address
        xm_setw(XR_DATA, 0x0fff);

        xm_setw(WR_INCR, 0);             // set write inc
        xm_setw(WR_ADDR, 0x4B00);        // set write address

        for (int i = 0; i < 16; i++)
        {
            xm_setw(DATA, 0x0000);        // set write inc

            uint16_t start;
            wait_vsync();
            uint16_t go = xm_getw(TIMER);
            while ((start = xm_getw(TIMER)) == go)
                ;
            xreg_setw(BLIT_RD_ADDR, 0x4B00);
            xreg_setw(BLIT_WR_ADDR, 0x4B01);
            xreg_setw(BLIT_COUNT, (320 * 240 / 4) - 2);
            wait_blit();
            uint16_t stop = xm_getw(TIMER) - start;
            dprintf("4bpp copy = 0x%04x (%u.%u) 1/10th ms\n", stop, stop / 10, stop % 10);
            dprintf("4bpp = 0x%04x (%u.%u) 1/10th ms\n", stop, stop / 10, stop % 10);
            wait_vsync();

            xm_setw(DATA, 0xFFFF);        // set write inc
            xreg_setw(BLIT_RD_ADDR, 0x4B00);
            xreg_setw(BLIT_WR_ADDR, 0x4B01);
            xreg_setw(BLIT_COUNT, (320 * 240 / 4) - 2);

            wait_blit();
            wait_vsync();
        }

        xm_setw(WR_INCR, 0);             // set write inc
        xm_setw(WR_ADDR, 0x0000);        // set write address

        for (int i = 0; i < 16; i++)
        {
            uint16_t v = i << 12 | i << 8 | i << 4 | i;
            xm_setw(DATA, v);        // set write inc

            uint16_t start;
            wait_vsync();
            uint16_t go = xm_getw(TIMER);
            while ((start = xm_getw(TIMER)) == go)
                ;

            xreg_setw(BLIT_RD_ADDR, 0x0000);
            xreg_setw(BLIT_WR_ADDR, 0x0001);
            xreg_setw(BLIT_COUNT, 0xFFFF);
            wait_blit();

            uint16_t stop = xm_getw(TIMER) - start;
            dprintf("black 64KW = 0x%04x (%u.%u) 1/10th ms\n", stop, stop / 10, stop % 10);

            xm_setw(DATA, 0x0000);        // set write inc

            go = xm_getw(TIMER);
            while ((start = xm_getw(TIMER)) == go)
                ;

            xreg_setw(BLIT_RD_ADDR, 0x0000);
            xreg_setw(BLIT_WR_ADDR, 0x0001);
            xreg_setw(BLIT_COUNT, 0xFFFF);
            wait_blit();

            stop = xm_getw(TIMER) - start;
            dprintf("white 64KW = 0x%04x (%u.%u) 1/10th ms\n", stop, stop / 10, stop % 10);
        }

    } while (false);
}

void test_dual_8bpp()
{
    const uint16_t width    = DRAW_WIDTH;
    const uint16_t height   = 200;
    uint16_t       old_copp = xreg_getw(COPP_CTRL);

    do
    {

        dprintf("test_dual_8pp\n");
        restore_colors();            // colormem A normal colors
        restore_colors2(0x8);        // colormem B normal colors (alpha 50%)

        uint16_t addrA = 0;             // start of VRAM
        uint16_t addrB = 0x8000;        // 2nd half of VRAM
        xm_setbl(SYS_CTRL, 0xf);

        // clear all VRAM

        uint16_t vaddr = 0;
        xm_setw(WR_INCR, 1);
        xm_setw(WR_ADDR, vaddr);
        do
        {
            xm_setw(DATA, 0);
        } while (++vaddr != 0);

        wait_vsync();
        xreg_setw(VID_CTRL, 0x0000);           // border color = black
        xreg_setw(PA_GFX_CTRL, 0x00FF);        // blank screen
        xreg_setw(PB_GFX_CTRL, 0x00FF);
        // install 320x200 "crop" copper list
        xm_setw(XR_ADDR, XR_COPPER_MEM);
        for (uint16_t i = 0; i < NUM_ELEMENTS(copper_320x200); i++)
        {
            xm_setw(XR_DATA, copper_320x200[i] >> 16);
            xm_setw(XR_DATA, copper_320x200[i] & 0xffff);
        }
        // set pf A 320x240 8bpp (cropped to 320x200)
        xreg_setw(PA_GFX_CTRL, 0x0065);
        xreg_setw(PA_TILE_CTRL, 0x000F);
        xreg_setw(PA_DISP_ADDR, addrA);
        xreg_setw(PA_LINE_LEN, DRAW_WORDS);
        xreg_setw(PA_HV_SCROLL, 0x0000);

        // set pf B 320x240 8bpp (cropped to 320x200)
        xreg_setw(PB_GFX_CTRL, 0x0065);
        xreg_setw(PB_TILE_CTRL, 0x000F);
        xreg_setw(PB_DISP_ADDR, addrB);
        xreg_setw(PB_LINE_LEN, DRAW_WORDS);
        xreg_setw(PB_HV_SCROLL, 0x0000);

        // enable copper
        wait_vsync();
        xmem_setw(XR_COPPER_MEM + (1 * 2) + 1, 0x0065);
        xmem_setw(XR_COPPER_MEM + (2 * 2) + 1, 0x00E5);
        xreg_setw(COPP_CTRL, 0x8000);

        uint16_t w = width;
        uint16_t x, y;
        x = 0;
        for (y = 0; y < height; y++)
        {
            int16_t len = w - x;
            if (x + len >= width)
            {
                len = width - x;
            }

            draw8bpp_h_line(addrA, ((y >> 2) + 1) & 0xff, x, y, len);

            w--;
            x++;
        }

        dprintf("Playfield A: 320x200 8bpp - horizontal-striped triangle + blanked B\n");
        if (delay_check(DELAY_TIME))
        {
            break;
        }

        wait_vsync();
        xmem_setw(XR_COPPER_MEM + (1 * 2) + 1, 0x0065);
        xmem_setw(XR_COPPER_MEM + (2 * 2) + 1, 0x0065);
        dprintf("Playfield A: 320x200 8bpp - horizontal-striped triangle + B enabled, but zeroed\n");
        if (delay_check(DELAY_TIME))
        {
            break;
        }

        w = height;
        y = 0;
        for (x = 0; x < width; x++)
        {
            int16_t len = w;
            if (len >= height)
            {
                len = height;
            }

            draw8bpp_v_line(addrB, ((x >> 2) + 1) & 0xff, x, y, len);
            w--;
        }

        wait_vsync();
        xmem_setw(XR_COPPER_MEM + (1 * 2) + 1, 0x00E5);
        xmem_setw(XR_COPPER_MEM + (2 * 2) + 1, 0x0065);
        dprintf("Playfield B: 320x200 8bpp - vertical-striped triangle, A blanked\n");
        if (delay_check(DELAY_TIME))
        {
            break;
        }

        wait_vsync();
        xmem_setw(XR_COPPER_MEM + (1 * 2) + 1, 0x0065);
        xmem_setw(XR_COPPER_MEM + (2 * 2) + 1, 0x0065);
        dprintf("Playfield A&B: mixed (alpha 0x8)\n");
        if (delay_check(DELAY_TIME))
        {
            break;
        }

        wait_vsync();
        restore_colors2(0x0);        // colormem B normal colors (alpha 0%)

        dprintf("Playfield A&B: colormap B alpha 0x0\n");
        if (delay_check(DELAY_TIME))
        {
            break;
        }

        wait_vsync();
        restore_colors2(0x4);        // colormem B normal colors (alpha 25%)

        dprintf("Playfield A&B: colormap B alpha 0x4\n");
        if (delay_check(DELAY_TIME))
        {
            break;
        }

        wait_vsync();
        restore_colors2(0x8);        // colormem B normal colors (alpha 50%)

        dprintf("Playfield A&B: colormap B alpha 0x8\n");
        if (delay_check(DELAY_TIME))
        {
            break;
        }

        wait_vsync();
        restore_colors2(0xF);        // colormem B normal colors (alpha 100%)

        dprintf("Playfield A&B: colormap B alpha 0xC\n");
        if (delay_check(DELAY_TIME))
        {
            break;
        }
    } while (false);

    dprintf("restore screen\n");
    restore_colors3();        // colormem B normal colors (alpha 0%)
    wait_vsync();
    xreg_setw(COPP_CTRL, 0x0000);
#if COPPER_TEST
    install_copper();
#endif
    xreg_setw(COPP_CTRL, old_copp);

    xreg_setw(PA_GFX_CTRL, 0x0000);
    xreg_setw(PB_GFX_CTRL, 0x0000);
    xreg_setw(PB_DISP_ADDR, 0xF000);
}

void test_hello()
{
    static const char test_string[] = "Xosera is mostly running happily on rosco_m68k";
    static uint16_t   test_read[sizeof(test_string)];

    xcls();
    xmsg(0, 0, 0xa, "WROTE:");
    xm_setw(WR_INCR, 1);                           // set write inc
    xm_setw(WR_ADDR, 0x0008);                      // set write address
    xm_setw(DATA, 0x0200 | test_string[0]);        // set full word
    for (size_t i = 1; i < sizeof(test_string) - 1; i++)
    {
        if (i == sizeof(test_string) - 5)
        {
            xm_setbh(DATA, 0x04);        // test setting bh only (saved, VRAM not altered)
        }
        xm_setbl(DATA, test_string[i]);        // set byte, will use continue using previous high byte (0x20)
    }

    // read test
    dprintf("Read VRAM test, with auto-increment.\n\n");
    dprintf(" Begin: rd_addr=0x0000, rd_inc=0x0001\n");
    xm_setw(RD_INCR, 1);
    xm_setw(RD_ADDR, 0x0008);
    uint16_t * tp = test_read;
    for (uint16_t c = 0; c < (sizeof(test_string) - 1); c++)
    {
        *tp++ = xm_getw(DATA);
    }
    uint16_t end_addr = xm_getw(RD_ADDR);

    xmsg(0, 2, 0xa, "READ:");
    xm_setw(WR_INCR, 1);                             // set write inc
    xm_setw(WR_ADDR, (text_columns * 2) + 8);        // set write address

    bool good = true;
    for (size_t i = 0; i < sizeof(test_string) - 1; i++)
    {
        uint16_t v = test_read[i];
        xm_setw(DATA, v);
        if ((v & 0xff) != test_string[i])
        {
            good = false;
        }
    }
    // incremented one extra, because data was already pre-read
    if (end_addr != sizeof(test_string) + 8)
    {
        good = false;
    }
    dprintf("   End: rd_addr=0x%04x.  Test: ", end_addr);
    dprintf("%s\n", good ? "good" : "BAD!");
}

void test_vram_speed()
{
    xcls();
    xv_prep();
    xm_setw(WR_INCR, 1);
    xm_setw(WR_ADDR, 0x0000);
    xm_setw(RD_INCR, 1);
    xm_setw(RD_ADDR, 0x0000);

    uint32_t vram_write = 0;
    uint32_t vram_read  = 0;
    uint32_t main_write = 0;
    uint32_t main_read  = 0;

    uint16_t reps = 16;        // just a few flashes for write test
    xmsg(0, 0, 0x02, "VRAM write     ");
    dprintf("VRAM write x %d\n", reps);
    uint32_t v = ((0x0f00 | 'G') << 16) | (0xf000 | 'o');
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t count = 0x8000;        // VRAM long count
        do
        {
            xm_setl(DATA, v);
        } while (--count);
        v ^= 0xff00ff00;
    }
    vram_write = timer_stop();
    global     = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    reps = 16;        // main ram test (NOTE: I am not even incrementing pointer below - like "fake
                      // register" write)
    xmsg(0, 0, 0x02, "main RAM write ");
    dprintf("main RAM write x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint32_t * ptr   = mem_buffer;
        uint16_t   count = 0x8000;        // VRAM long count
        do
        {
            //            *ptr++ = loop;    // GCC keeps trying to be clever, we want a fair test
            __asm__ __volatile__("move.l %[loop],(%[ptr])" : : [loop] "d"(loop), [ptr] "a"(ptr) :);
        } while (--count);
        v ^= 0xff00ff00;
    }
    main_write = timer_stop();
    global     = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    reps = 16;        // a bit longer read test (to show stable during read)
    xmsg(0, 0, 0x02, "VRAM read      ");
    dprintf("VRAM read x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t count = 0x8000;        // VRAM long count
        do
        {
            v = xm_getl(DATA);
        } while (--count);
    }
    vram_read = timer_stop();
    global    = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    reps = 16;        // main ram test (NOTE: I am not even incrementing pointer below - like "fake
                      // register" read)
    xmsg(0, 0, 0x02, "main RAM read  ");
    dprintf("main RAM read x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint32_t * ptr   = mem_buffer;
        uint16_t   count = 0x8000;        // VRAM long count
        do
        {
            //            v += *ptr++;    // GCC keeps trying to be clever, we want a fair test
            __asm__ __volatile__("move.l (%[ptr]),%[v]" : [v] "+d"(v) : [ptr] "a"(ptr) :);
        } while (--count);
        v ^= 0xff00ff00;
    }
    main_read = timer_stop();
    global    = v;         // save v so GCC doesn't optimize away test
    reps      = 32;        // a bit longer read test (to show stable during read)
    xmsg(0, 0, 0x02, "VRAM slow read ");
    dprintf("VRAM slow read x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t count = 0x8000;        // VRAM long count
        do
        {
            xm_setw(RD_ADDR, 0);
            v = xm_getbl(DATA);
        } while (--count);
    }
    vram_read = timer_stop();
    global    = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    reps = 16;        // a bit longer read test (to show stable during read)
    xmsg(0, 0, 0x02, "VRAM slow read2");
    dprintf("VRAM slow read2 x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t count = 0x8000;        // VRAM long count
        do
        {
            xm_setw(RD_ADDR, count & 0xff);
            v = xm_getbl(DATA);
        } while (--count);
    }
    vram_read = timer_stop();
    global    = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    dprintf("done\n");

    dprintf("MOVEP.L VRAM write      128KB x 16 (2MB)    %d ms (%d KB/sec)\n",
            vram_write,
            (1000U * 128 * reps) / vram_write);
    dprintf(
        "MOVEP.L VRAM read       128KB x 16 (2MB)    %u ms (%u KB/sec)\n", vram_read, (1000U * 128 * reps) / vram_read);
    dprintf("MOVE.L  main RAM write  128KB x 16 (2MB)    %u ms (%u KB/sec)\n",
            main_write,
            (1000U * 128 * reps) / main_write);
    dprintf(
        "MOVE.L  main RAM read   128KB x 16 (2MB)    %u ms (%u KB/sec)\n", main_read, (1000U * 128 * reps) / main_read);
}

#if 0        // TODO: needs recalibrating for VSync timer
uint16_t rosco_m68k_CPUMHz()
{
    uint32_t count;
    uint32_t tv;
    __asm__ __volatile__(
        "   moveq.l #0,%[count]\n"
        "   move.w  _TIMER_100HZ+2.w,%[tv]\n"
        "0: cmp.w   _TIMER_100HZ+2.w,%[tv]\n"
        "   beq.s   0b\n"
        "   move.w  _TIMER_100HZ+2.w,%[tv]\n"
        "1: addq.w  #1,%[count]\n"                   //   4  cycles
        "   cmp.w   _TIMER_100HZ+2.w,%[tv]\n"        //  12  cycles
        "   beq.s   1b\n"                            // 10/8 cycles (taken/not)
        : [count] "=d"(count), [tv] "=&d"(tv)
        :
        :);
    uint16_t MHz = (uint16_t)(((count * 26U) + 500U) / 1000U);
    dprintf("rosco_m68k: m68k CPU speed %d.%d MHz (%d.%d BogoMIPS)\n",
            MHz / 10,
            MHz % 10,
            count * 3 / 10000,
            ((count * 3) % 10000) / 10);

    return (uint16_t)((MHz + 5U) / 10U);
}
#endif

const char blurb[] =
    "\n"
    "Xosera is an FPGA based video adapter designed with the rosco_m68k retro\n"
    "computer in mind. Inspired in concept by it's \"namesake\" the Commander X16's\n"
    "VERA, Xosera is an original open-source video adapter design, built with open-\n"
    "source tools and is tailored with features generally appropriate for a Motorola\n"
    "68K era retro computer like the rosco_m68k (or even an 8-bit CPU).\n"
    "\n"
    "  \xf9  VGA or HDMI/DVI output at 848x480 or 640x480 (16:9 or 4:3 @ 60Hz)\n"
    "  \xf9  2 x 256 color palette out of 4096 colors (12-bit RGB)\n"
    "  \xf9  128KB of embedded video RAM (16-bit words @33/25 MHz)\n"
    "  \xf9  Register based interface with 16 16-bit registers\n"
    "  \xf9  Read/write VRAM with programmable read/write address increment\n"
    "  \xf9  Fast 8-bit bus interface (using MOVEP) for rosco_m68k (by Ross Bamford)\n"
    "  \xf9  Fonts writable in VRAM or in dedicated 8KB of font memory\n"
    "  \xf9  8x8 or 8x16 character tile size (or truncated e.g., 8x10)\n"
    "  \xf9  Tiled modes with 1024 glyphs, 16 or 256 colors and H & V mirrorring\n"
    "  \xf9  Horizontal and/or vertical pixel relpeat 1, 2, 3, 4x (e.g. 424x240 or 320x240)\n"
    "  \xf9  Smooth horizontal and vertical native pixel tile scrolling\n"
    "  \xf9  2-color full-res bitmap mode (with attribute per 8 pixels, ala Sinclair)\n"
    "  \xf9  TODO: Two 16 color \"planes\" or combined for 256 colors\n"
    "  \xf9  TODO: \"Blitter\" for fast VRAM copy & fill operations\n"
    "  \xf9  TODO: 2-D operations \"blitter\" with modulo and shifting/masking\n"
    "  \xf9  TODO: At least one \"cursor\" sprite (or more)\n"
    "  \xf9  TODO: Wavetable stereo audio (spare debug GPIOs for now)\n";

static void test_xr_read()
{
    dprintf("test_xr\n");

    xcls();

    xreg_setw(PB_GFX_CTRL, 0x0000);
    xreg_setw(PB_TILE_CTRL, 0x000F);
    xreg_setw(PB_DISP_ADDR, 0xF000);
    uint16_t vaddr = 0;
    xm_setw(WR_INCR, 1);
    for (vaddr = 0xF000; vaddr != 0x0000; vaddr++)
    {
        xm_setw(WR_ADDR, vaddr);
        xm_setw(DATA, vaddr - 0xF000);
    }
    xm_setw(WR_ADDR, 0xF000);
    xm_setw(DATA, 0x1f00 | 'P');
    xm_setw(DATA, 0x1f00 | 'L');
    xm_setw(DATA, 0x1f00 | 'A');
    xm_setw(DATA, 0x1f00 | 'Y');
    xm_setw(DATA, 0x1f00 | 'F');
    xm_setw(DATA, 0x1f00 | 'I');
    xm_setw(DATA, 0x1f00 | 'E');
    xm_setw(DATA, 0x1f00 | 'L');
    xm_setw(DATA, 0x1f00 | 'D');
    xm_setw(DATA, 0x1f00 | '-');
    xm_setw(DATA, 0x1f00 | 'B');


    xm_setw(WR_INCR, 1);
    for (vaddr = 0; vaddr < 0x2000; vaddr++)
    {
        xm_setw(WR_ADDR, vaddr);
        xm_setw(DATA, vaddr + 0x0100);
    }
    xm_setw(WR_ADDR, 0x000);
    xm_setw(DATA, 0x1f00 | 'V');
    xm_setw(DATA, 0x1f00 | 'R');
    xm_setw(DATA, 0x1f00 | 'A');
    xm_setw(DATA, 0x1f00 | 'M');


    if (delay_check(DELAY_TIME))
    {
        return;
    }

    for (uint16_t taddr = XR_TILE_MEM + 0x0800; taddr < XR_TILE_MEM + 0x1400; taddr++)
    {
        if (taddr < 0x0800 || taddr > 0x1000)
        {
            xm_setw(XR_ADDR, taddr);
            xm_setw(XR_DATA, taddr + 0x0100);
        }
    }
    xreg_setw(PA_DISP_ADDR, 0x0C00);
    xreg_setw(PA_TILE_CTRL, 0x020F);
    xm_setw(XR_ADDR, XR_TILE_MEM + 0x0C00);
    xm_setw(XR_DATA, 0x1f00 | 'T');
    xm_setw(XR_DATA, 0x1f00 | 'I');
    xm_setw(XR_DATA, 0x1f00 | 'L');
    xm_setw(XR_DATA, 0x1f00 | 'E');


    if (delay_check(DELAY_TIME))
    {
        return;
    }

    //    uint8_t oldint = mcDisableInterrupts();        // NOTE: should not be needed (and doesn't "solve" issues)
    for (int r = 0; r < 100; r++)
    {
        if (r == 50)
        {
            xreg_setw(PA_DISP_ADDR, 0x0000);
            xreg_setw(PA_TILE_CTRL, 0x000F);
        }
        for (int w = XR_TILE_MEM; w < XR_TILE_MEM + 0x1400; w++)
        {
            //            touching
            xm_setw(XR_ADDR, w);
            //            __asm__ __volatile__("nop ; nop ; nop ; nop");
            uint16_t v = xm_getw(XR_DATA);           // read tile mem
            xm_setw(XR_DATA, r & 1 ? v : ~v);        // toggle to prove read and set in VRAM
                                                     //            __asm__ __volatile__("nop ; nop ; nop ; nop");
        }

        if (delay_check(10))
        {
            return;
        }
    }
    //    mcEnableInterrupts(oldint);

    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_GFX_CTRL, 0x0000);         // set 8-BPP tiled (bad TILEMEM contention)
    xreg_setw(PA_TILE_CTRL, 0x000F);        // set 8-BPP tiled (bad TILEMEM contention)
    if (delay_check(DELAY_TIME * 2))
    {
        return;
    }
}

void set_alpha_slow(int alpha)
{
    uint16_t a = (alpha & 0xf) << 12;
    for (int i = XR_COLOR_MEM; i < XR_COLOR_MEM + 256; i++)
    {
        wait_vsync();
        xm_setw(XR_ADDR, i);
        __asm__ __volatile__("nop ; nop ; nop ; nop ; nop ; nop ; nop");
        uint16_t v = (xm_getw(XR_DATA) & 0xfff) | a;
        xm_setw(XR_DATA, v);
    }
}

static void set_alpha(int alpha)
{
    uint16_t a = (alpha & 0xf) << 12;
    for (int i = XR_COLOR_MEM; i < XR_COLOR_MEM + 256; i++)
    {
        xm_setw(XR_ADDR, i);
        while (xm_getbl(SYS_CTRL) & 0x40)
            ;
        uint16_t v = (xm_getw(XR_DATA) & 0xfff) | a;
        xm_setw(XR_DATA, v);
    }
}

uint32_t test_count;
void     xosera_test()
{
    // flush any input charaters to avoid instant exit
    while (checkchar())
    {
        readchar();
    }

    dprintf("Xosera_test_m68k\n");

    dprintf("\nxosera_init(0)...");
    bool success = xosera_init(0);
    xreg_setw(PA_GFX_CTRL, 0x0000);        // text mode (unblank)
    dprintf("%s (%dx%d)\n", success ? "succeeded" : "FAILED", xreg_getw(VID_HSIZE), xreg_getw(VID_VSIZE));

    // D'oh! Uses timer    rosco_m68k_CPUMHz();

#if 1
    dprintf("Installing interrupt handler...");
    install_intr();
    dprintf("okay.\n");

    dprintf("Checking for interrupt...");
    uint32_t t = XFrameCount;
    while (XFrameCount == t)
        ;
    dprintf("okay. Vsync interrupt detected.\n\n");
#else
    dprintf("NOT Installing interrupt handler\n");
#endif

#if COPPER_TEST
    install_copper();
#endif

    if (delay_check(4000))
    {
        return;
    }

    while (true)
    {
        uint32_t t = XFrameCount;
        uint32_t h = t / (60 * 60 * 60);
        uint32_t m = t / (60 * 60) % 60;
        uint32_t s = (t / 60) % 60;
        dprintf("*** xosera_test_m68k iteration: %u, running %u:%02u:%02u\n", test_count++, h, m, s);

        xcls();
        uint16_t version   = xreg_getw(VERSION);
        uint32_t githash   = ((uint32_t)xreg_getw(GITHASH_H) << 16) | (uint32_t)xreg_getw(GITHASH_L);
        uint16_t monwidth  = xreg_getw(VID_HSIZE);
        uint16_t monheight = xreg_getw(VID_VSIZE);
        uint16_t monfreq   = xreg_getw(VID_VFREQ);

        uint16_t gfxctrl  = xreg_getw(PA_GFX_CTRL);
        uint16_t tilectrl = xreg_getw(PA_TILE_CTRL);
        uint16_t dispaddr = xreg_getw(PA_DISP_ADDR);
        uint16_t linelen  = xreg_getw(PA_LINE_LEN);
        uint16_t hvscroll = xreg_getw(PA_HV_SCROLL);
        uint16_t sysctrl  = xm_getw(SYS_CTRL);

        dprintf(
            "Xosera v%1x.%02x #%08x Features:0x%02x\n", (version >> 8) & 0xf, (version & 0xff), githash, version >> 8);
        dprintf("Monitor Mode: %dx%d@%2x.%02xHz\n", monwidth, monheight, monfreq >> 8, monfreq & 0xff);
        dprintf("\nPlayfield A:\n");
        dprintf("PA_GFX_CTRL : 0x%04x PA_TILE_CTRL: 0x%04x\n", gfxctrl, tilectrl);
        dprintf("PA_DISP_ADDR: 0x%04x PA_LINE_LEN : 0x%04x\n", dispaddr, linelen);
        dprintf("PA_HV_SCROLL: 0x%04x\n", hvscroll);
        dprintf("\n");

        dprintf("SYS_CTRL: 0x%04x\n", sysctrl);
        xm_setw(SYS_CTRL, sysctrl);
        dprintf("SYS_CTRL: 0x%04x\n", sysctrl);

        restore_colors();

        xreg_setw(PB_GFX_CTRL, 0x0000);
        xreg_setw(PB_TILE_CTRL, 0x100F);
        xreg_setw(PB_DISP_ADDR, 0xF000);
        uint16_t vaddr = 0;
        xm_setw(WR_INCR, 1);
        for (vaddr = 0xF000; vaddr != 0x0000; vaddr++)
        {
            xm_setw(WR_ADDR, vaddr);
            xm_setw(DATA, vaddr);
        }
        xm_setw(WR_ADDR, 0xF000);
        xm_setw(DATA, 0x1f00 | 'P');
        xm_setw(DATA, 0x1f00 | 'L');
        xm_setw(DATA, 0x1f00 | 'A');
        xm_setw(DATA, 0x1f00 | 'Y');
        xm_setw(DATA, 0x1f00 | 'F');
        xm_setw(DATA, 0x1f00 | 'I');
        xm_setw(DATA, 0x1f00 | 'E');
        xm_setw(DATA, 0x1f00 | 'L');
        xm_setw(DATA, 0x1f00 | 'D');
        xm_setw(DATA, 0x1f00 | '-');
        xm_setw(DATA, 0x1f00 | 'B');

#if COPPER_TEST
        if (test_count & 1)
        {
            dprintf("Copper test disabled for this iteration.\n");
            restore_colors();
            xreg_setw(COPP_CTRL, 0x0000);
        }
        else
        {
            dprintf("Copper test enabled for this interation.\n");
            restore_colors();
            xreg_setw(COPP_CTRL, 0x8000);
        }

#endif

#if LR_MARGIN_TEST
        // crop left and right 10 pixels
        xreg_setw(VID_LEFT, 10);
        xreg_setw(VID_RIGHT, monwidth - 10);
#endif

        for (int y = 0; y < 30; y += 3)
        {
            xmsg(20, y, (y & 0xf) ? (y & 0xf) : 0xf0, ">>> Xosera rosco_m68k test utility <<<<");
        }

        if (delay_check(DELAY_TIME))
        {
            break;
        }

        if (SD_check_support())
        {
            dprintf("SD card supported: ");

            if (SD_FAT_initialize())
            {
                dprintf("SD card ready\n");
                use_sd = true;
            }
            else
            {
                dprintf("no SD card\n");
                use_sd = false;
            }
        }
        else
        {
            dprintf("No SD card support.\n");
        }

        if (use_sd)
        {
            test_blit();
        }

        test_dual_8bpp();

        test_xr_read();


        // 4/8 bpp test
        if (use_sd)
        {
            wait_vsync();
            xreg_setw(PA_GFX_CTRL, 0x0065);        // bitmap + 8-bpp + Hx2 + Vx2
            xreg_setw(PA_LINE_LEN, 160);

            load_sd_colors("/xosera_r1_pal.raw");
            load_sd_bitmap("/xosera_r1.raw", 0x0000);
            if (delay_check(DELAY_TIME))
            {
                break;
            }
            set_alpha(0xf);
            if (delay_check(DELAY_TIME))
            {
                break;
            }
        }

        // 4/8 bpp test
        if (use_sd)
        {
            wait_vsync();
            xreg_setw(PA_GFX_CTRL, 0x0065);        // bitmap + 8-bpp + Hx2 + Vx2
            xreg_setw(PA_LINE_LEN, 160);

            load_sd_colors("/color_cube_320x240_256_pal.raw");
            load_sd_bitmap("/color_cube_320x240_256.raw", 0x0000);
            if (delay_check(DELAY_TIME))
            {
                break;
            }
            set_alpha(0xf);
            if (delay_check(DELAY_TIME))
            {
                break;
            }
        }

        // 4/8 bpp test
        if (use_sd)
        {
            wait_vsync();
            xreg_setw(PA_GFX_CTRL, 0x0055);        // bitmap + 4-bpp + Hx2 + Vx2
            xreg_setw(PA_LINE_LEN, 80);

            load_sd_colors("/ST_KingTut_Dpaint_16_pal.raw");
            load_sd_bitmap("/ST_KingTut_Dpaint_16.raw", 0x0000);
            if (delay_check(DELAY_TIME))
            {
                break;
            }
            set_alpha(0xf);
            if (delay_check(DELAY_TIME))
            {
                break;
            }
        }

        if (use_sd)
        {
            wait_vsync();
            xreg_setw(PA_GFX_CTRL, 0x0055);        // bitmap + 4-bpp + Hx2 + Vx2
            xreg_setw(PA_LINE_LEN, 80);

            load_sd_colors("/escher-relativity_320x240_16_pal.raw");
            load_sd_bitmap("/escher-relativity_320x240_16.raw", 0x0000);
            if (delay_check(DELAY_TIME))
            {
                break;
            }
            set_alpha(0xf);
            if (delay_check(DELAY_TIME))
            {
                break;
            }
        }
        restore_colors();
        if (use_sd)
        {
            wait_vsync();
            xreg_setw(PA_GFX_CTRL, 0x0040);        // bitmap + 1-bpp + Hx1 + Vx1
            xreg_setw(PA_LINE_LEN, 80);

            load_sd_bitmap("/space_shuttle_color_small.raw", 0x0000);
            if (delay_check(DELAY_TIME))
            {
                break;
            }
            set_alpha(0xf);
            if (delay_check(DELAY_TIME))
            {
                break;
            }
        }

        set_alpha(0x0);
        if (use_sd)
        {
            wait_vsync();
            xreg_setw(PA_GFX_CTRL, 0x0040);        // bitmap + 1-bpp + Hx1 + Vx1
            xreg_setw(PA_LINE_LEN, 80);

            load_sd_bitmap("/mountains_mono_640x480w.raw", 0x0000);
            if (delay_check(DELAY_TIME))
            {
                break;
            }
        }

        if (use_sd)
        {
            wait_vsync();
            xreg_setw(PA_GFX_CTRL, 0x0040);        // bitmap + 1-bpp + Hx1 + Vx1
            xreg_setw(PA_LINE_LEN, 80);

            load_sd_bitmap("/escher-relativity_640x480w.raw", 0x0000);
            if (delay_check(DELAY_TIME))
            {
                break;
            }
        }

        wait_vsync();
        xreg_setw(PA_GFX_CTRL, 0x0000);
        test_hello();
        if (delay_check(DELAY_TIME))
        {
            break;
        }
#if 0        // bored with this test. :)
        test_vram_speed();
        if (delay_check(DELAY_TIME))
        {
            break;
        }
#endif
    }
    wait_vsync();

    xreg_setw(PA_GFX_CTRL, 0x0000);                           // text mode
    xreg_setw(PA_TILE_CTRL, 0x000F);                          // text mode
    xreg_setw(COPP_CTRL, 0x0000);                             // disable copper
    xreg_setw(PA_LINE_LEN, xreg_getw(VID_HSIZE) >> 3);        // line len
    restore_colors();
    remove_intr();
    xcls();
    xmsg(0, 0, 0x02, "Exited.");


    while (checkchar())
    {
        readchar();
    }
}
