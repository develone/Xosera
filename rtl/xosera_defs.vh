// xosera_defs.vh
//
// vim: set noet ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`ifndef XOSERA_DEFS_VH
`define XOSERA_DEFS_VH

// "hack" to allow quoted filename from define
`define STRINGIFY(x) `"x`"

`ifdef ICE40UP5K	// iCE40UltraPlus5K specific
// Lattice/SiliconBlue PLL "magic numbers" to derive pixel clock from 12Mhz oscillator (from "icepll" utility)
`ifdef	MODE_640x400	// 25.175 MHz (requested), 25.125 MHz (achieved)
	localparam PLL_DIVR	=	4'b0000;	// DIVR =  0
	localparam PLL_DIVF	=	7'b1000010;	// DIVF = 66
	localparam PLL_DIVQ	=	3'b101;		// DIVQ =  5
`elsif	MODE_640x480	// 25.175 MHz (requested), 25.125 MHz (achieved)
	localparam PLL_DIVR	=	4'b0000;	// DIVR =  0
	localparam PLL_DIVF	=	7'b1000010;	// DIVF = 66
	localparam PLL_DIVQ	=	3'b101;		// DIVQ =  5
`elsif	MODE_720x400	// 28.322 MHz (requested), 28.500 MHz (achieved)
	localparam PLL_DIVR	=	4'b0000;	// DIVR =  0
	localparam PLL_DIVF	=	7'b1001011;	// DIVF = 75
	localparam PLL_DIVQ	=	3'b101;		// DIVQ =  5
`elsif	MODE_848x480	// 33.750 MHz (requested), 33.750 MHz (achieved)
	localparam PLL_DIVR	=	4'b0000;	// DIVR =  0
	localparam PLL_DIVF	=	7'b0101100;	// DIVF = 44
	localparam PLL_DIVQ	=	3'b100;		// DIVQ =  4
`elsif	MODE_800x600	// 40.000 MHz (requested), 39.750 MHz (achieved) [tight timing]
	localparam PLL_DIVR	=	4'b0000;	// DIVR =  0
	localparam PLL_DIVF	=	7'b0110100;	// DIVF = 52
	localparam PLL_DIVQ	=	3'b100;		// DIVQ =  4
`elsif MODE_1024x768	// 65.000 MHz (requested), 65.250 MHz (achieved) [fails timing]
	localparam PLL_DIVR	=	4'b0000;	// DIVR =  0
	localparam PLL_DIVF	=	7'b1010110;	// DIVF = 86
	localparam PLL_DIVQ	=	3'b100;		// DIVQ =  4
`elsif MODE_1280x720	// 74.176 MHz (requested), 73.500 MHz (achieved) [fails timing]
	localparam PLL_DIVR	=	4'b0000;	// DIVR =  0
	localparam PLL_DIVF	=	7'b0110000;	// DIVF = 48
	localparam PLL_DIVQ	=	3'b011;		// DIVQ =  3
`else
	$error("No video mode set, see Makefile");
`endif
`endif

`ifdef	MODE_640x400
	// VGA mode 640x480 @ 60Hz (pixel clock 25.175Mhz)
	localparam PIXEL_FREQ		= 25_175_000;					// pixel clock in Hz
	localparam VISIBLE_WIDTH	= 640;							// horizontal active pixels
	localparam VISIBLE_HEIGHT	= 400;							// vertical active lines
	localparam H_FRONT_PORCH	= 16;							// H pre-sync (front porch) pixels
	localparam H_SYNC_PULSE		= 96;							// H sync pulse pixels
	localparam H_BACK_PORCH		= 48;							// H post-sync (back porch) pixels
	localparam V_FRONT_PORCH	= 12;							// V pre-sync (front porch) lines
	localparam V_SYNC_PULSE		= 2;							// V sync pulse lines
	localparam V_BACK_PORCH		= 35;							// V post-sync (back porch) lines
	localparam H_SYNC_POLARITY	= 1'b0;							// H sync pulse active level
	localparam V_SYNC_POLARITY	= 1'b1;							// V sync pulse active level

`elsif	MODE_640x480
	// VGA mode 640x480 @ 60Hz (pixel clock 25.175Mhz)
	localparam PIXEL_FREQ		= 25_175_000;					// pixel clock in Hz
	localparam VISIBLE_WIDTH	= 640;							// horizontal active pixels
	localparam VISIBLE_HEIGHT	= 480;							// vertical active lines
	localparam H_FRONT_PORCH	= 16;							// H pre-sync (front porch) pixels
	localparam H_SYNC_PULSE		= 96;							// H sync pulse pixels
	localparam H_BACK_PORCH		= 48;							// H post-sync (back porch) pixels
	localparam V_FRONT_PORCH	= 10;							// V pre-sync (front porch) lines
	localparam V_SYNC_PULSE		= 2;							// V sync pulse lines
	localparam V_BACK_PORCH		= 33;							// V post-sync (back porch) lines
	localparam H_SYNC_POLARITY	= 1'b0;							// H sync pulse active level
	localparam V_SYNC_POLARITY	= 1'b0;							// V sync pulse active level

`elsif	MODE_720x400
	// VGA mode 720x400 @ 70Hz (pixel clock 28.322Mhz)
	localparam PIXEL_FREQ		= 28_322_000;					// pixel clock in Hz
	localparam VISIBLE_WIDTH	= 720;							// horizontal active pixels
	localparam VISIBLE_HEIGHT	= 400;							// vertical active lines
	localparam H_FRONT_PORCH	= 18;							// H pre-sync (front porch) pixels
	localparam H_SYNC_PULSE		= 108;							// H sync pulse pixels
	localparam H_BACK_PORCH		= 54;							// H post-sync (back porch) pixels
	localparam V_FRONT_PORCH	= 12;							// V pre-sync (front porch) lines
	localparam V_SYNC_PULSE		= 2;							// V sync pulse lines
	localparam V_BACK_PORCH		= 35;							// V post-sync (back porch) lines
	localparam H_SYNC_POLARITY	= 1'b0;							// H sync pulse active level
	localparam V_SYNC_POLARITY	= 1'b1;							// V sync pulse active level

`elsif	MODE_848x480
	// VGA mode 848x480 @ 60Hz (pixel clock 33.750Mhz)
	localparam PIXEL_FREQ		= 33_750_000;					// pixel clock in Hz
	localparam VISIBLE_WIDTH	= 848;							// horizontal active pixels
	localparam VISIBLE_HEIGHT	= 480;							// vertical active lines
	localparam H_FRONT_PORCH	= 16;							// H pre-sync (front porch) pixels
	localparam H_SYNC_PULSE		= 112;							// H sync pulse pixels
	localparam H_BACK_PORCH		= 112;							// H post-sync (back porch) pixels
	localparam V_FRONT_PORCH	= 6;							// V pre-sync (front porch) lines
	localparam V_SYNC_PULSE		= 8;							// V sync pulse lines
	localparam V_BACK_PORCH		= 23;							// V post-sync (back porch) lines
	localparam H_SYNC_POLARITY	= 1'b1;							// H sync pulse active level
	localparam V_SYNC_POLARITY	= 1'b1;							// V sync pulse active level

`elsif	MODE_800x600
	// VGA mode 800x600 @ 60Hz (pixel clock 40.000Mhz)
	localparam PIXEL_FREQ		= 40_000_000;					// pixel clock in Hz
	localparam VISIBLE_WIDTH	= 800;							// horizontal active pixels
	localparam VISIBLE_HEIGHT	= 600;							// vertical active lines
	localparam H_FRONT_PORCH	= 40;							// H pre-sync (front porch) pixels
	localparam H_SYNC_PULSE		= 128;							// H sync pulse pixels
	localparam H_BACK_PORCH		= 88;							// H post-sync (back porch) pixels
	localparam V_FRONT_PORCH	= 1;							// V pre-sync (front porch) lines
	localparam V_SYNC_PULSE		= 4;							// V sync pulse lines
	localparam V_BACK_PORCH		= 23;							// V post-sync (back porch) lines
	localparam H_SYNC_POLARITY	= 1'b1;							// H sync pulse active level
	localparam V_SYNC_POLARITY	= 1'b1;							// V sync pulse active level

`elsif	MODE_1024x768
	// VGA mode 1024x768 @ 60Hz (pixel clock 65.000Mhz)
	localparam PIXEL_FREQ		= 65_000_000;					// pixel clock in Hz
	localparam VISIBLE_WIDTH	= 1024;							// horizontal active pixels
	localparam VISIBLE_HEIGHT	= 768;							// vertical active lines
	localparam H_FRONT_PORCH	= 24;							// H pre-sync (front porch) pixels
	localparam H_SYNC_PULSE		= 136;							// H sync pulse pixels
	localparam H_BACK_PORCH		= 160;							// H post-sync (back porch) pixels
	localparam V_FRONT_PORCH	= 3;							// V pre-sync (front porch) lines
	localparam V_SYNC_PULSE		= 6;							// V sync pulse lines
	localparam V_BACK_PORCH		= 29;							// V post-sync (back porch) lines
	localparam H_SYNC_POLARITY	= 1'b0;							// H sync pulse active level
	localparam V_SYNC_POLARITY	= 1'b0;							// V sync pulse active level

`elsif	MODE_1280x720
	// VGA mode 1280x720 @ 60Hz (pixel clock 74.250Mhz)
	localparam PIXEL_FREQ		= 74_250_000;					// pixel clock in Hz
	localparam VISIBLE_WIDTH	= 1280;							// horizontal active pixels
	localparam VISIBLE_HEIGHT	= 720;							// vertical active lines
	localparam H_FRONT_PORCH	= 110;							// H pre-sync (front porch) pixels
	localparam H_SYNC_PULSE		= 40;							// H sync pulse pixels
	localparam H_BACK_PORCH		= 220;							// H post-sync (back porch) pixels
	localparam V_FRONT_PORCH	= 5;							// V pre-sync (front porch) lines
	localparam V_SYNC_PULSE		= 5;							// V sync pulse lines
	localparam V_BACK_PORCH		= 20;							// V post-sync (back porch) lines
	localparam H_SYNC_POLARITY	= 1'b1;							// H sync pulse active level
	localparam V_SYNC_POLARITY	= 1'b1;							// V sync pulse active level

`else
	$error("No video mode set, see Makefile");
`endif

	// calculated video mode parameters
	localparam TOTAL_WIDTH		= H_FRONT_PORCH + H_SYNC_PULSE + H_BACK_PORCH + VISIBLE_WIDTH;
	localparam TOTAL_HEIGHT		= V_FRONT_PORCH + V_SYNC_PULSE + V_BACK_PORCH + VISIBLE_HEIGHT;
	localparam OFFSCREEN_WIDTH	= TOTAL_WIDTH - VISIBLE_WIDTH;
	localparam OFFSCREEN_HEIGHT	= TOTAL_HEIGHT - VISIBLE_HEIGHT;

	// character font related constants
	localparam FONT_WIDTH       = 8;							// 8 pixels wide character tiles (1 byte)
	localparam FONT_HEIGHT		= 16;							// up to 16 pixels high character tiles
	localparam FONT_CHARS		= 256;							// number of character tiles per font
	localparam CHARS_WIDE		= (VISIBLE_WIDTH/FONT_WIDTH);
	localparam CHARS_HIGH		= (VISIBLE_HEIGHT/FONT_HEIGHT);
	localparam FONT_SIZE        = (FONT_CHARS * FONT_HEIGHT);	// bytes per font (up to 8x16 character tiles)

`endif	// `ifndef XOSERA_DEFS_VH
