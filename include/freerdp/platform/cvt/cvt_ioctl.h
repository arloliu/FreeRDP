/*
 * Copyright (c) 2011 by Cloudvue Technologies, Inc.  ALL RIGHTS RESERVED.
 * These coded instructions, statements, and computer programs are the copyrighted
 * works and confidential proprietary information of Cloudvue Technologies, Inc.
 * They may not be modified, copied, reproduced, distributed, or disclosed
 * to third parties in any manner, medium, or form, in whole or in part,
 * without the prior written consent of Cloudvue Technologies, Inc.
 */

#ifndef __CVT_IOCTL_H__
#define __CVT_IOCTL_H__

#include "cvt_define.h"


/*******************************************
 * Device System Call (Input/Ouput Control)
 *******************************************/
#define DEV_IOCTL_TYPE              0x83      /* from CT8311 --> use 83 */

/* regular ioctls */
#define DEV_IOCTL_DECODER_SETUP     _IO(DEV_IOCTL_TYPE, 0x01)
#define DEV_IOCTL_DECODING          _IO(DEV_IOCTL_TYPE, 0x02)
#define DEV_IOCTL_FRAME_ACTION      _IO(DEV_IOCTL_TYPE, 0x03)
#define DEV_IOCTL_MONITOR_RES       _IO(DEV_IOCTL_TYPE, 0x04)
#define DEV_IOCTL_DRIVER_RES        _IO(DEV_IOCTL_TYPE, 0x05)
#define DEV_IOCTL_RESOLUTION_SET    _IO(DEV_IOCTL_TYPE, 0x06)
#define DEV_IOCTL_CURSOR_SET        _IO(DEV_IOCTL_TYPE, 0x07)
#define DEV_IOCTL_DECODER_CLEANUP   _IO(DEV_IOCTL_TYPE, 0x08)
#define DEV_IOCTL_FILL_RECTS        _IO(DEV_IOCTL_TYPE, 0x09)
#define DEV_IOCTL_COPY_AREA         _IO(DEV_IOCTL_TYPE, 0x0a)
#define DEV_IOCTL_IMAGE_WRITE       _IO(DEV_IOCTL_TYPE, 0x0b)
#define DEV_IOCTL_MONITOR_SET       _IO(DEV_IOCTL_TYPE, 0x0c)
#define DEV_IOCTL_OSD_SET	    _IO(DEV_IOCTL_TYPE, 0x0d)
/* for diagnostics or internal use */
#define DEV_IOCTL_REG_OP            _IO(DEV_IOCTL_TYPE, 0x11) // access Cloudvue chip's register
#define DEV_IOCTL_SYS_OP            _IO(DEV_IOCTL_TYPE, 0x12) // access any system's physical address
#define DEV_IOCTL_DMA               _IO(DEV_IOCTL_TYPE, 0x13)
#define DEV_IOCTL_MEMCPY            _IO(DEV_IOCTL_TYPE, 0x14)
#define DEV_IOCTL_BAR_CONFIG        _IO(DEV_IOCTL_TYPE, 0x15)
#define DEV_IOCTL_DEV_MEM_MAP       _IO(DEV_IOCTL_TYPE, 0x16)
#define DEV_IOCTL_DECODER_INFO      _IO(DEV_IOCTL_TYPE, 0x17)
#define DEV_IOCTL_BLT               _IO(DEV_IOCTL_TYPE, 0x18)
#define DEV_IOCTL_MULTI_BLT         _IO(DEV_IOCTL_TYPE, 0x19)
#define DEV_IOCTL_DIRECT_BLT        _IO(DEV_IOCTL_TYPE, 0x1a)
#define DEV_IOCTL_IMAGE_DMA         _IO(DEV_IOCTL_TYPE, 0x1b)
#define DEV_IOCTL_IMAGE_MEMCPY      _IO(DEV_IOCTL_TYPE, 0x1c)
#define DEV_IOCTL_I2C_OP            _IO(DEV_IOCTL_TYPE, 0x1d) // access devices via Cloudvue's I2C interface



typedef enum
{
  READ_OP = 0,
  WRITE_OP
} _accessType;


typedef enum
{
  SIZE_8 = 0,                   /* 8-bit  */
  SIZE_16,                      /* 16-bit */
  SIZE_32                       /* 32-bit */
} _accessSize;


typedef enum                    /* Base Address Register */
{
  BAR_0 = 0,
  BAR_1,
  BAR_2,
  BAR_3
} _BARnum;

typedef enum
{
  CVT_FRAME_BEGIN = 0,
  CVT_FRAME_END
} _FA;

/******************
 * Data Structures
 ******************/

#define MONITOR_0 0x01
#define MONITOR_1 0x02
#define MONITOR_A 0x03


/*****************************************************************************/
typedef struct _monitor {
  cvU32 idx;                    /* display_0 (0) or display_1 (1) */
  cvU32 monitor_modes;          /* resolutions that monitor can support */
} monitor_st;


/*****************************************************************************/
#define MON_LAYOUT_S0     0x00  /* single monitor on display 0 */
#define MON_LAYOUT_S1     0x01  /* single monitor on display 1 */
#define MON_LAYOUT_L0_R1  0x02  /* dual monitors, display 0 on the left side, 
                                   display 1 on the right side */
#define MON_LAYOUT_L1_R0  0x03  /* dual monitors, display 1 on the left side, 
                                   display 0 on the right side */
#define MON_LAYOUT_T0_B1  0x04  /* dual monitors, display 0 on the top side, 
                                   display 1 on the bottom side */
#define MON_LAYOUT_T1_B0  0x05  /* dual monitors, display 1 on the top side, 
                                   display 0 on the bottom side */
#define MON_LAYOUT_CLONE  0x06  /* dual monitors, both show the same content */

typedef struct _resolution {
  cvU32 width;                  /* width of resolution, eg. 1024 */
  cvU32 height;                 /* height of resolution, eg. 768 */
  cvU32 bpp;                    /* bit-per-pixel, e.g. 32bpp, ... */
  cvU32 vxres;                  /* virtual screen horizontal size in pixels */
  cvU32 vyres;                  /* virtual screen virtical size in pixels */
} resolution_st;
/*****************************************************************************/
#define RDSH     0
#define RDSH_OPT 1
#define RDVH     2
#define RDVH_OPT 3

typedef struct _dec_setup {
  cvU32 width;                  /* width of resolution, eg. 1024 */
  cvU32 height;                 /* height of resolution, eg. 768 */
  cvU32 op_mode;                /* RDSH/RDSH_OPT/RDVH/RDVH_OPT */
} dec_setup_st;

typedef struct _mon_set {
  int   mon_layout;		/* monitor layout 
				   MON_LAYOUT_S0/MON_LAYOUT_S1/MON_LAYOUT_L0_R1/
				   MON_LAYOUT_L1_R0/MON_LAYOUT_T0_B1/
                                   MON_LAYOUT_T1_B0/MON_LAYOUT_CLONE */
  int   mon_rate;		/* monitor refresh rate */
} mon_set_st;

/*****************************************************************************/
typedef struct _dma {
  _accessType type;             /* READ or WRITE */
  cvU8        *p_usr_buf;       /* ptr to userspace buffer */
  cvU32       dev_buf;          /* buffer in device memory */
  cvU32       size;             /* number of bytes to be DMAed */
} dma_st;


/*****************************************************************************/
typedef struct _memcpy {
  _accessType type;             /* READ or WRITE */
  cvU8        *p_usr_buf;       /* ptr to userspace buffer */
  cvU32       dev_buf;          /* buffer in device memory */
  cvU32       size;             /* number of bytes to be DMAed */
} memcpy_st;


/*****************************************************************************/
typedef struct _reg {
  _accessType type;             /* READ or WRITE */
  cvU32       addr;             /* register address */
  cvU32       data;             /* data to be written or data returned */
  _accessSize size;             /* 8-bit, 16-bit, or 32-bit */
} reg_st;


/*****************************************************************************/
typedef struct _mem {
  _accessType type;             /* READ or WRITE */
  cvU32       start_addr;       /* start address */
  cvU32       length;           /* number of bytes being read or written */
}mem_st;


/*****************************************************************************/
typedef struct _i2c {           /* currently supports only 8-bit.  Can be modified
                                 * to support 16, or 32-bit, if necessary.
                                 */
  _accessType type;             /* READ or WRITE */
  cvU32       dev_id;           /* device id */
  cvU32       addr;             /* address being accessed */
  cvU32       data;             /* data to be written or data returned */
} i2c_st;


/*****************************************************************************/
typedef struct _BAR {
  _accessType type;             /* READ or WRITE */
  _BARnum     idx;              /* BAR0, BAR1, BAR2, or BAR3 */
  cvU32       offset;
  _accessSize size;
  cvU32       value;
} bar_st;


/*****************************************************************************/
#define CURSOR_AND_MASK         (BIT0)                /* 'b000000001 */
#define CURSOR_XOR_MASK         (BIT1)                /* 'b000000010 */
#define CURSOR_MONO             (BIT2)                /* 'b000000100 */
#define CURSOR_COLOR            (BIT3)                /* 'b000001000 */
#define CURSOR_COLOR_ALPHA      (BIT4)                /* 'b000010000 */
#define CURSOR_COLOR_DATA       (BIT5)                /* 'b000100000 */
#define CURSOR_OFFSET_XY        (BIT6)                /* 'b001000000 */
#define CURSOR_POS_XY           (BIT7)                /* 'b010000000 */
#define CURSOR_DISABLE          (BIT8)                /* 'b100000000 */

typedef struct _setCur {
  cvU32 x;                      /* screen's coordinate x */
  cvU32 y;                      /* screen's coordinate y */
  cvU32 offset_x;               /* offset x within the cursor block */
  cvU32 offset_y;               /* offset y within the cursor block */
  cvU32 status;                 /* which cursor's features are ON/OFF */
  cvU32 *p_AND_mask;            /* ptr to AND-mask buffer */
  cvU32 *p_XOR_mask;            /* ptr to XOR-mask buffer */
  cvU32 *p_CAX;                 /* ptr to color CAX buffer */
  cvU32 disp_idx;
} setCur_st;


/*****************************************************************************/
typedef enum
{
  RGB8888,                      // ARGB 32-bit
  YUV444                        // YUV planar, 8 bit per component
} blt_format;

typedef enum
{
  RGB888,			// RGB888
  MONO				// mono pattern
} pattern_format;


typedef struct _blt {
  cvU32      src_base;          // RGB or Y source base
  cvU32      src_u_base;        // U source base; only for YUV2RGB
  cvU32      src_v_base;        // V source base; only for YUV2RGB
  cvU32      src_width;         // source surface width in pixel
  cvU32      src_height;        // source surface height in lines
  cvU32      src_stride;        // source surface stride in bytes
  blt_format src_format;	// RGB or YUV
  cvU8       signYUV;		// YUV bias

  cvU32      dst_base;          // RGB destination base
  cvU32      dst_width;         // destination surface width in pixel
  cvU32      dst_height;        // destination surface height in lines
  cvU32      dst_stride;        // destination surface stride in bytes
  blt_format dst_format;        // should only be RGB

  cvS32      src_x;             // left start coord. of blt in src surface
  cvS32      src_y;             // top start coord. of blt in src surface
  cvS32      dst_x;             // left start coord. of blt in dst surface
  cvS32      dst_y;             // top start coord. of blt in dst surface
  cvU32      src_x_size;        // src blt region
  cvU32      src_y_size;        // src blt region
  cvU32      dst_x_size;        // dst blt region
  cvU32      dst_y_size;        // dst blt region
  cvU32      clip_left;         // left of clip region; draw inclusive
  cvU32      clip_top;          // top of clip region; draw inclusive
  cvU32      clip_right;        // right of clip region; draw exclusive
  cvU32      clip_bottom;       // bottom of clip region; draw exclusive
  
  cvU8	     pat_trans_mode;	// pattern transfer mode
  cvU32	     transparent_color;	// pattern transparent color
  cvU8       pat_off_x;		// pattern dimension offset-x
  cvU8       pat_off_y;		// pattern dimension offset-y
  cvU32      mono_pat_buf_low;	// mono pattern buffer low Dword
  cvU32      mono_pat_buf_high;	// mono pattern buffer high Dword
  cvU32      mono_pat_fg_color;	// mono pattern foreground color
  cvU32      mono_pat_bg_color;	// mono pattern background color
  pattern_format pat_format;    // RGB or mono
  
  int 	     draw_line;		// line draw
  int        last_draw;	      	// draw last pixel

  cvU8       rop;               // raster operation
} blt_st;

typedef struct _mem_map
{
  cvU32 display_addr[2];	// 2 displays
  cvU32 display_buf_stride[2];	// for 2 displays
  cvU32 cursor_addr;		// should be the same cursor for the two displays
  cvU32 comp_addr[MAX_SYNC];	// compressed tile data of frames
  cvU32 dec_y_addr[MAX_SYNC];	// decompressed y-data of frames
  cvU32 dec_u_addr[MAX_SYNC];	// decompressed u-data of frames
  cvU32 dec_v_addr[MAX_SYNC];	// decompressed v-data of frames
} mem_map_st;

typedef struct _multi_blt_rect
{
  cvU32 x;
  cvU32 y;
  cvU32 width;
  cvU32 height;
  cvU32 cmd_fifo_flag;
} multi_blt_rect_st;


typedef struct _multi_blt {
  cvU32      src_base;          // RGB or Y source base
  cvU32      src_u_base;        // U source base; only for YUV2RGB
  cvU32      src_v_base;        // V source base; only for YUV2RGB
  cvU32      src_width;         // source surface width in pixel
  cvU32      src_height;        // source surface height in lines
  cvU32      src_stride;        // source surface stride in bytes
  blt_format src_format;	// RGB or YUV
  cvU8       signYUV;		// YUV bias

  cvU32      dst_base;          // RGB destination base
  cvU32      dst_width;         // destination surface width in pixel
  cvU32      dst_height;        // destination surface height in lines
  cvU32      dst_stride;        // destination surface stride in bytes
  blt_format dst_format;        // should only be RGB

  cvU32      num_rects;         // number of rectangles
  multi_blt_rect_st *p_rects_src;
  multi_blt_rect_st *p_rects_dst;

  cvU32      clip_left;         // left of clip region; draw inclusive
  cvU32      clip_top;          // top of clip region; draw inclusive
  cvU32      clip_right;        // right of clip region; draw exclusive
  cvU32      clip_bottom;       // bottom of clip region; draw exclusive
  
  cvU8	     pat_trans_mode;	// pattern transfer mode
  cvU32	     transparent_color;	// pattern transparent color
  cvU8       pat_off_x;		// pattern dimension offset-x
  cvU8       pat_off_y;		// pattern dimension offset-y
  cvU32      mono_pat_buf_low;	// mono pattern buffer low Dword
  cvU32      mono_pat_buf_high;	// mono pattern buffer high Dword
  cvU32      mono_pat_fg_color;	// mono pattern foreground color
  cvU32      mono_pat_bg_color;	// mono pattern background color
  pattern_format pat_format;    // RGB or mono
  
  int 	     draw_line;		// line draw
  int        last_draw;	      	// draw last pixel

  cvU8       rop;               // raster operation
} multi_blt_st;

#define DIRECTBLT_CREATE 0x01
#define DIRECTBLT_APPEND 0x02
#define DIRECTBLT_GOGOGO 0x04

typedef struct _direct_blt
{
  cvU8*      p_src_buf;		// source data buffer
  cvU32      flag;	
  cvU32      x;
  cvU32	     y;
  cvU32	     width;
  cvU32      height;
  cvU32      display_num;
} direct_blt_st;

#define IMAGE_FORMAT_PIXMAP 0
#define IMAGE_FORMAT_BITMAP 1

typedef struct _image_copy_st
{
  cvU8*      p_src_buf;		// source data buffer
	
  cvU32      x;
  cvU32	     y;
  cvU32	     width;
  cvU32      height;
  cvU32      display_num;
  cvU32      image_format;
} image_copy_st;

typedef image_copy_st image_memcpy_st;
typedef image_copy_st image_dma_st;


/*****************************************************************************/
typedef struct _tile
{
  cvU32 size;                   /* tile's size */
  cvU32 *p_tile_data;           /* ptr to tile's data */
} tile_st;


/* NOTE: should ALWAYS the same as structure frm_rect_st in cvt_dev.h */
typedef struct _rect
{
  cvU16 x;
  cvU16 y;
  cvU16 width;
  cvU16 height;
} rect_st;


typedef struct _clip
{
  int left;
  int top;
  int right;
  int bottom;
} clip_st;

typedef struct _rfx_cvt_rect
{
	rect_st rect;
	rect_st clip;
} rfx_cvt_rect;

typedef struct _decode
{
  cvU8    *p_frame_data;        /* ptr to frame's data (in userspace) */
  cvU32   data_size;            /* size for frame's data */
  cvU8    quants[15];           /* quantization values table */
                                /* quants[ 0] Y{HL0,HH0} */ 
                                /* quants[ 1] Y{HH1,LH0} */
                                /* quants[ 2] Y{LH1,HL1} */
                                /* quants[ 3] Y{HL2,HH2} */
                                /* quants[ 4] Y{LL2,LH1} */
                                /* quants[ 5] U{HL0,HH0} */
                                /* quants[ 6] U{HH1,LH0} */
                                /* quants[ 7] U{LH1,HL1} */
                                /* quants[ 8] U{HL2,HH2} */
                                /* quants[ 9] U{LL2,LH1} */
                                /* quants[10] V{HL0,HH0} */
                                /* quants[11] V{HH1,LH0} */
                                /* quants[12] V{LH1,HL1} */
                                /* quants[13] V{HL2,HH2} */
                                /* quants[14] V{LL2,LH1} */
  cvU8    display_num;          /* display number */
  cvU16   dest_off_x;           /* destination offset x */
  cvU16   dest_off_y;           /* destination offset y */
  cvU32   num_tiles;            /* number of tiles in this frame */
  cvU32   *p_size;              /* ptr to array contains tiles' sizes */
  cvU32   num_tile_rects;       /* number of tile rectangles */
  rfx_cvt_rect *p_tile_rects;   /* ptr to set of tile rectangles' coordinates */
  cvU32   num_copy_rects;       /* number of copy rectangles */
  rfx_cvt_rect *p_copy_rects;   /* ptr to set of copy rectangles' coordinates */
} decode_st;

typedef struct _frame_action
{
  int     frame_action;
} frame_action_st;

typedef struct _clip_rect
{
  cvU32  x1;
  cvU32  y1;
  cvU32  x2;
  cvU32  y2;
} clip_rect_st;

typedef struct _fill_rects
{
  cvS16  x;
  cvS16  y;
  cvU16  cx;
  cvU16  cy;

  cvU32  color;
  cvU32  rop;
  cvU32  num_clips;
  clip_rect_st  *p_clips; 
} fr_st;

typedef struct _copy_area
{
  cvS16  srcx;
  cvS16  srcy;
  cvS16  dstx;
  cvS16  dsty;
  cvU16  cx;
  cvU16  cy;

  cvU32  num_clips;
  clip_rect_st *p_clips; 
} ca_st;


typedef struct _image_write_st
{
  cvU8*      p_src_buf;		// source data buffer
  cvS16      srcx;
  cvS16      srcy;
  cvU16      srcstride;
  cvS16      dstx;
  cvS16      dsty;
  cvU16	     srcwidth;
  cvU16      srcheight;
  cvU16	     dstwidth;
  cvU16      dstheight;
  cvU32      num_clips;
  clip_rect_st  *p_clips; 
  cvU32      rop;

  cvU32      image_format;
} image_write_st;

typedef struct _osd_st
{
	cvU32		enable;
	cvU8*		p_src_buf;
	cvU16		width;
	cvU16		height;
	cvS16		dstx;
	cvS16		dsty;
	cvU32		num_clips;
	clip_rect_st	*p_clips;
	cvU8		display_num;
} osd_st;

#endif  /* __CVT_IOCTL_H__ */
