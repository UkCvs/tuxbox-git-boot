/*---------------------------------------------------------------------------- */
/* */
/*  File generated by S1D13706CFG.EXE */
/* */
/*  Copyright (c) 2000,2001 Epson Research and Development, Inc. */
/*  All rights reserved. */
/* */
/*---------------------------------------------------------------------------- */

/* Panel: 320x240x8bpp 70Hz Color Single STN 8-bit (PCLK=6.250MHz) (Format 2) */

#define S1D_DISPLAY_WIDTH           320
#define S1D_DISPLAY_HEIGHT          240
#define S1D_DISPLAY_BPP             8
#define S1D_DISPLAY_SCANLINE_BYTES  320
#define S1D_PHYSICAL_VMEM_ADDR      0x800A0000L
#define S1D_PHYSICAL_VMEM_SIZE      0x14000L
#define S1D_PHYSICAL_REG_ADDR       0x80080000L
#define S1D_PHYSICAL_REG_SIZE       0x100
#define S1D_DISPLAY_PCLK            6250
#define S1D_PALETTE_SIZE            256
#define S1D_REGDELAYOFF             0xFFFE
#define S1D_REGDELAYON              0xFFFF

#define S1D_WRITE_PALETTE(p,i,r,g,b)  \
{  \
    ((volatile S1D_VALUE*)(p))[0x0A/sizeof(S1D_VALUE)] = (S1D_VALUE)((r)>>4);  \
    ((volatile S1D_VALUE*)(p))[0x09/sizeof(S1D_VALUE)] = (S1D_VALUE)((g)>>4);  \
    ((volatile S1D_VALUE*)(p))[0x08/sizeof(S1D_VALUE)] = (S1D_VALUE)((b)>>4);  \
    ((volatile S1D_VALUE*)(p))[0x0B/sizeof(S1D_VALUE)] = (S1D_VALUE)(i);  \
}

#define S1D_READ_PALETTE(p,i,r,g,b)  \
{  \
    ((volatile S1D_VALUE*)(p))[0x0F/sizeof(S1D_VALUE)] = (S1D_VALUE)(i);  \
    r = ((volatile S1D_VALUE*)(p))[0x0E/sizeof(S1D_VALUE)];  \
    g = ((volatile S1D_VALUE*)(p))[0x0D/sizeof(S1D_VALUE)];  \
    b = ((volatile S1D_VALUE*)(p))[0x0C/sizeof(S1D_VALUE)];  \
}

typedef unsigned short S1D_INDEX;
typedef unsigned char  S1D_VALUE;


typedef struct
{
    S1D_INDEX Index;
    S1D_VALUE Value;
} S1D_REGS;

static S1D_REGS aS1DRegs[] =
{


    {0x04,0x10},   /* BUSCLK MEMCLK Config Register */
#if 0
    {0x05,0x32},   /* PCLK Config  Register  */
#endif
    {0x10,0xD0},   /* PANEL Type Register */
    {0x11,0x00},   /* MOD Rate Register */
#if 0
    {0x12,0x34},   /* Horizontal Total Register */
#endif
    {0x14,0x27},   /* Horizontal Display Period Register */
    {0x16,0x00},   /* Horizontal Display Period Start Pos Register 0 */
    {0x17,0x00},   /* Horizontal Display Period Start Pos Register 1 */
    {0x18,0xF0},   /* Vertical Total Register 0  */
    {0x19,0x00},   /* Vertical Total Register 1 */
    {0x1C,0xEF},   /* Vertical Display Period Register 0 */
    {0x1D,0x00},   /* Vertical Display Period Register 1 */
    {0x1E,0x00},   /* Vertical Display Period Start Pos Register 0 */
    {0x1F,0x00},   /* Vertical Display Period Start Pos Register 1 */
    {0x20,0x87},   /* Horizontal Sync Pulse Width Register */
    {0x22,0x00},   /* Horizontal Sync Pulse Start Pos Register 0 */
    {0x23,0x00},   /* Horizontal Sync Pulse Start Pos Register 1 */
    {0x24,0x80},   /* Vertical Sync Pulse Width Register */
    {0x26,0x01},   /* Vertical Sync Pulse Start Pos Register 0 */
    {0x27,0x00},   /* Vertical Sync Pulse Start Pos Register 1 */
    {0x70,0x83},   /* Display Mode Register */
    {0x71,0x00},   /* Special Effects Register */
    {0x74,0x00},   /* Main Window Display Start Address Register 0 */
    {0x75,0x00},   /* Main Window Display Start Address Register 1 */
    {0x76,0x00},   /* Main Window Display Start Address Register 2 */
    {0x78,0x50},   /* Main Window Address Offset Register 0 */
    {0x79,0x00},   /* Main Window Address Offset Register 1 */
    {0x7C,0x00},   /* Sub Window Display Start Address Register 0 */
    {0x7D,0x00},   /* Sub Window Display Start Address Register 1 */
    {0x7E,0x00},   /* Sub Window Display Start Address Register 2 */
    {0x80,0x50},   /* Sub Window Address Offset Register 0 */
    {0x81,0x00},   /* Sub Window Address Offset Register 1 */
    {0x84,0x00},   /* Sub Window X Start Pos Register 0 */
    {0x85,0x00},   /* Sub Window X Start Pos Register 1 */
    {0x88,0x00},   /* Sub Window Y Start Pos Register 0 */
    {0x89,0x00},   /* Sub Window Y Start Pos Register 1 */
    {0x8C,0x4F},   /* Sub Window X End Pos Register 0 */
    {0x8D,0x00},   /* Sub Window X End Pos Register 1 */
    {0x90,0xEF},   /* Sub Window Y End Pos Register 0 */
    {0x91,0x00},   /* Sub Window Y End Pos Register 1 */
    {0xA0,0x00},   /* Power Save Config Register */
    {0xA1,0x00},   /* CPU Access Control Register */
    {0xA2,0x00},   /* Software Reset Register */
    {0xA3,0x00},   /* BIG Endian Support Register */
    {0xA4,0x00},   /* Scratch Pad Register 0 */
    {0xA5,0x00},   /* Scratch Pad Register 1 */
    {0xA8,0x01},   /* GPIO Config Register 0 */
    {0xA9,0x80},   /* GPIO Config Register 1 */
    {0xAC,0x01},   /* GPIO Status Control Register 0 */
    {0xAD,0x00},   /* GPIO Status Control Register 1 */
    {0xB0,0x00},   /* PWM CV Clock Control Register */
    {0xB1,0x00},   /* PWM CV Clock Config Register */
    {0xB2,0x00},   /* CV Clock Burst Length Register */
    {0xB3,0x00},   /* PWM Clock Duty Cycle Register */
    {0xAD,0x80},   /* reset seq */
	{0x70,0x03},   /*  */
};
