/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Cloudvue CVT8311 ASIC RemoteFX Codec
 *
 * Copyright 2012 Cloudvue Technologies Corp.
 * Copyright 2012 Arlo Liu <arlo.liu@atrustcorp.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include <freerdp/codec/rfx.h>
#include <freerdp/utils/memory.h>
#include <freerdp/constants.h>
#include <freerdp/settings.h>
#include <freerdp/platform.h>
#include <freerdp/platform/cvt/cvt.h>

#include "rfx_constants.h"
#include "rfx_types.h"
#include "rfx_utils.h"
#include "rfx_platform_cvt.h"


static uint32 g_num_rects_tile[2] = {0, 0};
static uint8 g_idx[2] = {0, 0};
static rfx_cvt_rect g_rects_tile[2][2][1024];	/* tile rects , one for previous, the other for current */
static rfx_cvt_rect g_rects_copy[2][1024];	/* tiles need to copy from previous display buffer */

static int rfx_cvt_is_rect_in_rect(RFX_RECT *p1, RFX_RECT *p2)
{
	if ( (p1->x >= p2->x) && (p1->y >= p2->y) &&
		(p1->x+p1->width  <= p2->x+p2->width ) &&
		(p1->y+p1->height <= p2->y+p2->height) )
		return 1;
	else
		return 0;
}

static int rfx_cvt_is_overlapping(RFX_RECT *p1, RFX_RECT *p2)
{
	if (
		(p1->x+p1->width  <= p2->x) || (p1->x >= p2->x+p2->width) ||
		(p1->y+p1->height <= p2->y) || (p1->y >= p2->y+p2->height)
	)
		return 0;
	else
		return 1;

}

static int rfx_cvt_get_clip(RFX_MESSAGE* message, RFX_RECT *tile_rect, rfx_cvt_rect *p_rects_dst, rfx_cvt_rect *p_rects_ext)
{
	int i;
	RFX_RECT m_rect;
	int num_match = 0;
	rfx_cvt_rect *dst_rect = p_rects_dst;
	rfx_cvt_rect *ext_rect = p_rects_ext;

	for(i=0;i<message->num_rects;i++)
	{
 		m_rect.x      = message->rects[i].x;
 		m_rect.y      = message->rects[i].y;
 		m_rect.width  = message->rects[i].width;
 		m_rect.height = message->rects[i].height;
		if (rfx_cvt_is_overlapping(tile_rect,&m_rect))
		{
			if (num_match)
			{
				ext_rect->rect.x     = tile_rect->x;
				ext_rect->rect.y     = tile_rect->y;
				ext_rect->rect.width = tile_rect->width;
				ext_rect->rect.height= tile_rect->height;
				ext_rect->clip.x     = m_rect.x;
				ext_rect->clip.y     = m_rect.y;
				ext_rect->clip.width = m_rect.width;
				ext_rect->clip.height= m_rect.height;
				ext_rect++;
			}
			else
			{
				dst_rect->rect.x     = tile_rect->x;
				dst_rect->rect.y     = tile_rect->y;
				dst_rect->rect.width = tile_rect->width;
				dst_rect->rect.height= tile_rect->height;
				dst_rect->clip.x     = m_rect.x;
				dst_rect->clip.y     = m_rect.y;
				dst_rect->clip.width = m_rect.width;
				dst_rect->clip.height= m_rect.height;
			}
			num_match++;
		}
	}
	return num_match;
}

static inline void rfx_cvt_setup_decode_st(decode_st* dec_st, uint16 destLeft, uint16 destTop, RFX_CONTEXT* context, uint8 monitor_layout)
{
	switch (monitor_layout)
	{
		case MON_LAYOUT_S0:
		case MON_LAYOUT_CLONE:
			dec_st->display_num = 0;
			break;
		case MON_LAYOUT_S1:
			dec_st->display_num = 1;
			break;
		case MON_LAYOUT_L0_R1:
			if (destLeft < context->width)
			{
				dec_st->display_num = 0;
			}
			else
			{
				dec_st->display_num = 1;
			}
			break;
		case MON_LAYOUT_L1_R0:
			if (destLeft < context->width)
			{
				dec_st->display_num = 1;
			}
			else
			{
				dec_st->display_num = 0;
			}
			break;
		case MON_LAYOUT_T0_B1:
			if (destTop < context->height)
			{
				dec_st->display_num = 0;
			}
			else
			{
				dec_st->display_num = 1;
			}
			break;
		case MON_LAYOUT_T1_B0:
			if (destTop < context->height)
			{
				dec_st->display_num = 1;
			}
			else
			{
				dec_st->display_num = 0;
			}
			break;
		default:
			dec_st->display_num = 0;
			break;
	}
}

static void rfx_cvt_process_message_tileset(RFX_CONTEXT* context, RFX_MESSAGE* message, STREAM* s)
{
	int i, j;
	uint16 destLeft, destTop;
	RFX_CONTEXT_CVT_PRIV* priv;
	uint16 subtype;
	uint32 blockLen;
	uint32 blockType;
	uint32 tilesDataSize;
	uint32* quants;
	uint8 quant;
	int pos;
	int YQidx,UQidx,VQidx;
	decode_st dec_st;
	RFX_RECT *p_rects;
	uint32 *p_size;
	int io_err;
	uint16 x,y;
	uint32 num_rects_max;
	uint32 num_tile_rects_tmp;
	uint32 num_match;
	rfx_cvt_rect *p_rects_dst;
	rfx_cvt_rect *p_rects_ext;

	destLeft = message->dst_rect.left;
	destTop = message->dst_rect.top;
	priv = (RFX_CONTEXT_CVT_PRIV*) context->priv;

	stream_read_uint16(s, subtype); /* subtype (2 bytes) must be set to CBT_TILESET (0xCAC2) */

	if (subtype != CBT_TILESET)
	{
		DEBUG_WARN("invalid subtype, expected CBT_TILESET.");
		return;
	}

	stream_seek_uint16(s); /* idx (2 bytes), must be set to 0x0000 */
	stream_seek_uint16(s); /* properties (2 bytes) */

	stream_read_uint8(s, context->num_quants); /* numQuant (1 byte) */
	stream_seek_uint8(s); /* tileSize (1 byte), must be set to 0x40 */

	if (context->num_quants < 1)
	{
		DEBUG_WARN("no quantization value.");
		return;
	}

	stream_read_uint16(s, message->num_tiles); /* numTiles (2 bytes) */
	if (message->num_tiles < 1)
	{
		DEBUG_WARN("no tiles.");
		return;
	}

	stream_read_uint32(s, tilesDataSize); /* tilesDataSize (4 bytes) */
	if (context->quants != NULL)
		context->quants = (uint32*) xrealloc((void*) context->quants, context->num_quants * 5 * sizeof(uint32));
	else
		context->quants = (uint32*) xmalloc(context->num_quants * 5 * sizeof(uint32));

	quants = context->quants;

	/* quantVals */
	for (i = 0; i < context->num_quants; i++)
	{
		/* RFX_CODEC_QUANT */
		/* Hardware takes packed Q values. */
		stream_read_uint8(s, *quants++);
		stream_read_uint8(s, *quants++);
		stream_read_uint8(s, *quants++);
		stream_read_uint8(s, *quants++);
		stream_read_uint8(s, *quants++);
	}

	memset(&dec_st,0,sizeof(dec_st));

	rfx_cvt_setup_decode_st(&dec_st, destLeft, destTop, context, priv->rfx_mon_layout);

	dec_st.dest_off_x = destLeft;
	dec_st.dest_off_y = destTop;

	dec_st.p_frame_data = stream_get_tail(s);
	dec_st.num_tiles = message->num_tiles;
	dec_st.p_size = (uint32*) xmalloc(dec_st.num_tiles * sizeof(uint32));

	p_size = dec_st.p_size;

	dec_st.data_size = 0;

	dec_st.num_tile_rects = message->num_tiles;
	p_rects = (RFX_RECT*) xmalloc(dec_st.num_tile_rects * sizeof(RFX_RECT));
	num_tile_rects_tmp = 0;
	/* tiles */
	for (i = 0; i < dec_st.num_tiles; i++)
	{
		/* RFX_TILE */
		stream_read_uint16(s, blockType); /* blockType (2 bytes), must be set to CBT_TILE (0xCAC3) */
		stream_read_uint32(s, blockLen); /* blockLen (4 bytes) */

		pos = stream_get_pos(s) - 6 + blockLen;

		if (blockType != CBT_TILE)
		{
			printf("unknown block type 0x%X, expected CBT_TILE (0xCAC3).", blockType);
			break;
		}

		/* hardware only takes 1 set of Qvalues per frame, check the first tile */
		stream_read_uint8(s, YQidx);
		stream_read_uint8(s, UQidx);
		stream_read_uint8(s, VQidx);
		if (i == 0)
		{
			if ((YQidx > context->num_quants) ||
				(UQidx > context->num_quants) ||
				(VQidx > context->num_quants))
			{
				printf("invalid Qval index.\n");
				printf("YQidx=%d;UQidx=%d;VQidx=%d\n",YQidx,UQidx,VQidx);
				return;
			}
			for(j=0;j<5;j++)
			{
				quant = *(uint8*) (context->quants + 5 * YQidx + j);
				dec_st.quants[4-j]  = (quant >> 4) | (quant << 4);
				quant = *(uint8*) (context->quants + 5 * UQidx + j);
				dec_st.quants[9-j]  = (quant >> 4) | (quant << 4);
				quant = *(uint8*) (context->quants + 5 * VQidx + j);
				dec_st.quants[14-j] = (quant >> 4) | (quant << 4);
			}
		}
		stream_read_uint16(s, x);
		stream_read_uint16(s, y);
		p_rects->x	= x*64;
		p_rects->y	= y*64;
		p_rects->width	= 64;
		p_rects->height	= 64;
		//horizontal merge tiles
		if (i)
		{
			if (
				(p_rects->y == (p_rects-1)->y) &&
				(p_rects->x == (p_rects-1)->x + (p_rects-1)->width)
			)
			{
				p_rects--;
				p_rects->width += 64;
				num_tile_rects_tmp--;
			}
		}
		p_rects++;
		num_tile_rects_tmp++;
		dec_st.data_size+=blockLen;
		*(p_size++)=blockLen;

		stream_set_pos(s, pos);
	}

	dec_st.num_tile_rects = num_tile_rects_tmp;
	p_rects -= num_tile_rects_tmp;
	/* verical merge tiles */
	for(j=0;j<dec_st.num_tile_rects;j++)
	{
		for(i=j+1;i<dec_st.num_tile_rects;i++)
		{
			if (
				(p_rects[j].x == p_rects[i].x) &&
				((p_rects[j].y+p_rects[j].height) == p_rects[i].y) &&
				(p_rects[j].width == p_rects[i].width) && (p_rects[i].width!=0)
			)
			{
				p_rects[j].height +=64;
				p_rects[i].width = 0;
			}
		}
	}
	num_tile_rects_tmp=0;
	for(i=0;i<dec_st.num_tile_rects;i++)
	{
		if (p_rects[i].width)
		{
			if (i!=num_tile_rects_tmp)
				memcpy((uint8*)(p_rects+num_tile_rects_tmp),(uint8*)(p_rects+i),sizeof(rect_st));
			num_tile_rects_tmp++;
		}
	}
	dec_st.num_tile_rects = num_tile_rects_tmp;
	/* create clip window list*/
	num_rects_max = message->num_rects*dec_st.num_tile_rects;
	dec_st.p_tile_rects = (rfx_cvt_rect*) xmalloc(num_rects_max * sizeof(rfx_cvt_rect));
	p_rects_dst = dec_st.p_tile_rects;
	p_rects_ext = dec_st.p_tile_rects + dec_st.num_tile_rects; // for extra rects when 1 tile belongs to more than 1 rects.
	num_tile_rects_tmp = 0;
	for (i = 0; i < dec_st.num_tile_rects; i++)
	{
		num_match = rfx_cvt_get_clip(message, p_rects, p_rects_dst, p_rects_ext);
		if (!num_match)
		{
			printf("interesting ???\n");
			p_rects_dst->clip.x      = p_rects->x ;
			p_rects_dst->clip.y      = p_rects->y ;
			p_rects_dst->clip.width  = p_rects->width ;
			p_rects_dst->clip.height = p_rects->height ;
			p_rects_dst++;
			num_tile_rects_tmp++;
		}
		else
		{
			p_rects_dst++;
			p_rects_ext+=(num_match-1);
		}
		p_rects++;
		num_tile_rects_tmp += num_match;
	}
	p_rects-=dec_st.num_tile_rects;
	xfree(p_rects);

	dec_st.num_tile_rects = num_tile_rects_tmp;
	dec_st.num_copy_rects = 0;

	if (dec_st.num_tile_rects > RFX_CVT_MAX_DECODE_TILES)
		printf("******Error : too many rectangles ( %d )******\n",dec_st.num_tile_rects);

	//Call driver
	io_err = ioctl(priv->fd, DEV_IOCTL_DECODING, &dec_st);
	if (io_err)
		printf("Hardware decoding failed.\n");

	xfree(dec_st.p_tile_rects);
	xfree(dec_st.p_size);

}

static int rfx_cvt_rects_compare(rfx_cvt_rect *p1, rfx_cvt_rect *p2)
{
	if ( rfx_cvt_is_rect_in_rect((RFX_RECT*)&p1->rect,(RFX_RECT*)&p1->clip) &&
		rfx_cvt_is_rect_in_rect((RFX_RECT*)&p2->rect,(RFX_RECT*)&p2->clip) )
	{
		if ( (p1->rect.x == p2->rect.x) && (p1->rect.y == p2->rect.y) &&
			(p1->rect.width == p2->rect.width) && (p1->rect.height == p2->rect.height) )
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if ( (p1->rect.x == p2->rect.x) && (p1->rect.y == p2->rect.y) &&
			(p1->rect.width == p2->rect.width) && (p1->rect.height == p2->rect.height) &&
		 	(p1->clip.x == p2->clip.x) && (p1->clip.y == p2->clip.y) &&
			(p1->clip.width == p2->clip.width) && (p1->clip.height == p2->clip.height) )
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
}

static int rfx_cvt_rdvh_opt_tile_merge(rfx_cvt_rect *p_src, rfx_cvt_rect *p_dst, uint32 num)
{
	int i,j;
	int num_after_merge=0;
	int num_after_h_merge;

	//horizontal merge
	for(i=0;i<num;i++)
	{
		if (i==0)
		{
			if (p_src!=p_dst)
				memcpy((uint8*)&p_dst[0],(uint8*)&p_src[0],sizeof(rfx_cvt_rect));
			num_after_merge++;
		}
		else if (
				((p_src+i)->clip.x == (p_src+i-1)->clip.x) && ((p_src+i)->clip.y == (p_src+i-1)->clip.y) &&
				((p_src+i)->clip.width == (p_src+i-1)->clip.width) && ((p_src+i)->clip.height == (p_src+i-1)->clip.height) &&
				((p_src+i)->rect.y == (p_src+i-1)->rect.y) && ((p_src+i)->rect.x == ((p_src+i-1)->rect.x + (p_src+i-1)->rect.width)))
			{
				(p_dst+num_after_merge-1)->rect.width += 64;
			}
		else
		{
			if ((i!=num_after_merge)||(p_dst!=p_src))
				memcpy((uint8*)&p_dst[num_after_merge],(uint8*)&p_src[i],sizeof(rfx_cvt_rect));
			num_after_merge++;
		}
	}

	num_after_h_merge = num_after_merge;

	/* verical merge tiles */
	for(j=0;j<num_after_h_merge;j++)
	{
		for(i=j+1;i<num_after_h_merge;i++)
		{
			if (
				(p_dst[j].clip.x == p_dst[i].clip.x) && (p_dst[j].clip.y == p_dst[i].clip.y) &&
				(p_dst[j].clip.width == p_dst[i].clip.width) && (p_dst[j].clip.height == p_dst[i].clip.height) &&
				(p_dst[j].rect.x == p_dst[i].rect.x) && ((p_dst[j].rect.y+p_dst[j].rect.height) == p_dst[i].rect.y) &&
				(p_dst[j].rect.width == p_dst[i].rect.width) && (p_dst[i].rect.width!=0)
			)
			{
				p_dst[j].rect.height +=64;
				p_dst[i].rect.width = 0;
			}
		}
	}
	num_after_merge=0;
	for(i=0;i<num_after_h_merge;i++)
	{
		if (p_dst[i].rect.width)
		{
			if (i!=num_after_merge)
				memcpy((uint8*)&p_dst[num_after_merge],(uint8*)&p_dst[i],sizeof(rfx_cvt_rect));
			num_after_merge++;
		}
	}
	return num_after_merge;

}

static int rfx_cvt_rdvh_opt_copy_rects_optimize(rfx_cvt_rect *p_rect, uint32 num)
{
	int i,j;

	for(i=1,j=0;i<num;i++)
	{
		if (
			((p_rect+i)->rect.x == (p_rect+j)->rect.x) && ((p_rect+i)->rect.y == (p_rect+j)->rect.y) &&
			((p_rect+i)->rect.width == (p_rect+j)->rect.width) && ((p_rect+i)->rect.height == (p_rect+j)->rect.height)

		)
		{
			(p_rect+j)->clip.x	= (p_rect+j)->rect.x;
			(p_rect+j)->clip.y	= (p_rect+j)->rect.y;
			(p_rect+j)->clip.width	= (p_rect+j)->rect.width;
			(p_rect+j)->clip.height	= (p_rect+j)->rect.height;
		}
		else
		{
			memcpy((uint8*)&p_rect[++j],(uint8*)&p_rect[i],sizeof(rfx_cvt_rect));
		}
	}
	return j+1;
}

static void rfx_cvt_rdvh_opt_process_message_tileset(RFX_CONTEXT* context, RFX_MESSAGE* message, STREAM* s)
{
	int i,j;
	RFX_CONTEXT_CVT_PRIV* priv;
	uint16 destLeft, destTop;
	uint16 subtype;
	uint32 blockLen;
	uint32 blockType;
	uint32 tilesDataSize;
	uint32* quants;
	uint8 quant;
	int pos;
	int YQidx,UQidx,VQidx;
	decode_st dec_st;
	rect_st *p_rects;
	uint32 *p_size;
	int io_err;
	uint16 x,y;
	uint32 num_tile_rects;
	uint32 num_tile_rects_save;
	uint32 num_match;
	uint32 num_copy_rects;
	rfx_cvt_rect *p_rects_dst;
	rfx_cvt_rect *p_rects_ext;
	rfx_cvt_rect *p1,*p2;
	int match;
	uint8 disp_idx;
	uint8 idx;
	RFX_RECT *p_tile_rects;
	uint32 num_rects_max;

	destLeft = message->dst_rect.left;
	destTop = message->dst_rect.top;
	priv = (RFX_CONTEXT_CVT_PRIV*) context->priv;

	stream_read_uint16(s, subtype); /* subtype (2 bytes) must be set to CBT_TILESET (0xCAC2) */

	if (subtype != CBT_TILESET)
	{
		DEBUG_WARN("invalid subtype, expected CBT_TILESET.");
		return;
	}

	stream_seek_uint16(s); /* idx (2 bytes), must be set to 0x0000 */
	stream_seek_uint16(s); /* properties (2 bytes) */

	stream_read_uint8(s, context->num_quants); /* numQuant (1 byte) */
	stream_seek_uint8(s); /* tileSize (1 byte), must be set to 0x40 */

	if (context->num_quants < 1)
	{
		DEBUG_WARN("no quantization value.");
		return;
	}

	stream_read_uint16(s, message->num_tiles); /* numTiles (2 bytes) */

	if (message->num_tiles < 1)
	{
		DEBUG_WARN("no tiles.");
		return;
	}

	stream_read_uint32(s, tilesDataSize); /* tilesDataSize (4 bytes) */

	if (context->quants != NULL)
		context->quants = (uint32*) xrealloc((void*) context->quants, context->num_quants * 5 * sizeof(uint32));
	else
		context->quants = (uint32*) xmalloc(context->num_quants * 5 * sizeof(uint32));

	quants = context->quants;

	/* quantVals */
	for (i = 0; i < context->num_quants; i++)
	{
		/* RFX_CODEC_QUANT */
		/* Hardware takes packed Q values. */
		stream_read_uint8(s, *quants++);
		stream_read_uint8(s, *quants++);
		stream_read_uint8(s, *quants++);
		stream_read_uint8(s, *quants++);
		stream_read_uint8(s, *quants++);
	}

	memset(&dec_st,0,sizeof(dec_st));

	rfx_cvt_setup_decode_st(&dec_st, destLeft, destTop, context, priv->rfx_mon_layout);

	dec_st.dest_off_x = destLeft;
	dec_st.dest_off_y = destTop;

	disp_idx = dec_st.display_num;
	idx = g_idx[disp_idx];

	dec_st.p_frame_data = stream_get_tail(s);
	dec_st.num_tiles = message->num_tiles;
	dec_st.p_size = (uint32*) xmalloc(dec_st.num_tiles * sizeof(uint32));

	p_size = dec_st.p_size;

	dec_st.data_size = 0;

	num_tile_rects_save = 0;
	num_tile_rects = 0;
	p_rects_dst = &g_rects_tile[disp_idx][idx][0];
	p_rects_ext = p_rects_dst + dec_st.num_tiles; // for extra rects when 1 tile belongs to more than 1 rects.
	p_tile_rects = (RFX_RECT*) xmalloc(dec_st.num_tiles * sizeof(RFX_RECT));
	/* tiles */
	for (i = 0; i < dec_st.num_tiles; i++)
	{
		/* RFX_TILE */
		stream_read_uint16(s, blockType); /* blockType (2 bytes), must be set to CBT_TILE (0xCAC3) */
		stream_read_uint32(s, blockLen); /* blockLen (4 bytes) */

		pos = stream_get_pos(s) - 6 + blockLen;

		if (blockType != CBT_TILE)
		{
			printf("unknown block type 0x%X, expected CBT_TILE (0xCAC3).", blockType);
			break;
		}

		/* hardware only takes 1 set of Qvalues per frame, check the first tile */
		stream_read_uint8(s, YQidx);
		stream_read_uint8(s, UQidx);
		stream_read_uint8(s, VQidx);
		if (i == 0)
		{
			if ((YQidx > context->num_quants) ||
				(UQidx > context->num_quants) ||
				(VQidx > context->num_quants))
			{
				printf("invalid Qval index.\n");
				printf("YQidx=%d;UQidx=%d;VQidx=%d\n",YQidx,UQidx,VQidx);
				return;
			}
			for(j=0;j<5;j++)
			{
				quant = *(uint8*) (context->quants + 5 * YQidx + j);
				dec_st.quants[4-j]  = (quant >> 4) | (quant << 4);
				quant = *(uint8*) (context->quants + 5 * UQidx + j);
				dec_st.quants[9-j]  = (quant >> 4) | (quant << 4);
				quant = *(uint8*) (context->quants + 5 * VQidx + j);
				dec_st.quants[14-j] = (quant >> 4) | (quant << 4);
			}
		}
		stream_read_uint16(s, x);
		stream_read_uint16(s, y);

		/* save tile rectangles for comparison */
		p_rects = &(g_rects_tile[disp_idx][idx][i].rect) ;
		p_rects->x      = x*64;
		p_rects->y      = y*64;
		p_rects->width  = 64;
		p_rects->height = 64;

		/* get clip window ( get clips tile by tile ) */
		num_match = rfx_cvt_get_clip(message, (RFX_RECT*)p_rects, p_rects_dst, p_rects_ext);
		if (!num_match)
		{
			printf("interesting ???\n");
			p_rects_dst->clip.x      = p_rects->x ;
			p_rects_dst->clip.y      = p_rects->y ;
			p_rects_dst->clip.width  = p_rects->width ;
			p_rects_dst->clip.height = p_rects->height ;
			p_rects_dst++;
			num_tile_rects_save++;
		}
		else
		{
			p_rects_dst++;
			p_rects_ext+=(num_match-1);
		}
		num_tile_rects_save += num_match;

		/* tile_rects for blit */
		p_tile_rects->x      = x*64;
		p_tile_rects->y      = y*64;
		p_tile_rects->width  = 64;
		p_tile_rects->height = 64;

		//horizontal merge tiles
		if (i)
		{
			if (
				(p_tile_rects->y == (p_tile_rects-1)->y) &&
				(p_tile_rects->x == (p_tile_rects-1)->x + (p_tile_rects-1)->width)
			)
			{
				p_tile_rects--;
				p_tile_rects->width += 64;
				num_tile_rects--;
			}
		}
		p_tile_rects++;
		num_tile_rects++;

		dec_st.data_size+=blockLen;
		*(p_size++)=blockLen;

		stream_set_pos(s, pos);
	}

	dec_st.num_tile_rects = num_tile_rects;
	p_tile_rects -= num_tile_rects;
	/* check tiles need to be copied from previous frame */
	num_copy_rects = 0;
	for(i=0;i<g_num_rects_tile[disp_idx];i++)
	{
		match = 0;
		for(j=0;j<num_tile_rects_save;j++)
		{
			if (rfx_cvt_rects_compare(&(g_rects_tile[disp_idx][1-idx][i]),&(g_rects_tile[disp_idx][idx][j])))
			{
				match = 1;
				break;
			}
		}
		if (!match)
		{
			memcpy((uint8*)&(g_rects_copy[disp_idx][num_copy_rects]),(uint8*)&(g_rects_tile[disp_idx][1-idx][i]),sizeof(rfx_cvt_rect));
			num_copy_rects++;
		}
	}
	/* update tile rects number (for next frame) */
	g_num_rects_tile[disp_idx] = num_tile_rects_save;

	/* horizontal merge copy rects */
	p1 = (rfx_cvt_rect*)&g_rects_copy[disp_idx][0];
	p2 = (rfx_cvt_rect*)&g_rects_copy[disp_idx][0];
	if (num_copy_rects)
	{
		dec_st.num_copy_rects = rfx_cvt_rdvh_opt_tile_merge(p1,p2,num_copy_rects);
		dec_st.num_copy_rects = rfx_cvt_rdvh_opt_copy_rects_optimize(p1,dec_st.num_copy_rects);
	}
	else
	{
		dec_st.num_copy_rects = 0;
	}
	dec_st.p_copy_rects = &(g_rects_copy[disp_idx][0]);

	/* exceed 2D command fifo depth limitation */
	if (dec_st.num_copy_rects > 160)
	{
		dec_st.num_copy_rects = 1;
		dec_st.p_copy_rects->rect.x = 0;
		dec_st.p_copy_rects->rect.y = 0;
		dec_st.p_copy_rects->rect.width  = context->width;
		dec_st.p_copy_rects->rect.height = context->height;
		dec_st.p_copy_rects->clip.x = 0;
		dec_st.p_copy_rects->clip.y = 0;
		dec_st.p_copy_rects->clip.width  = context->width;
		dec_st.p_copy_rects->clip.height = context->height;
	}

	/* verical merge tile rects */
	for(j=0;j<dec_st.num_tile_rects;j++)
	{
		for(i=j+1;i<dec_st.num_tile_rects;i++)
		{
			if (
				(p_tile_rects[j].x == p_tile_rects[i].x) &&
				((p_tile_rects[j].y+p_tile_rects[j].height) == p_tile_rects[i].y) &&
				(p_tile_rects[j].width == p_tile_rects[i].width) && (p_tile_rects[i].width!=0)
			)
			{
				p_tile_rects[j].height +=64;
				p_tile_rects[i].width = 0;
			}
		}
	}
	num_tile_rects=0;
	for(i=0;i<dec_st.num_tile_rects;i++)
	{
		if (p_tile_rects[i].width)
		{
			if (i!=num_tile_rects)
				memcpy((uint8*)(p_tile_rects+num_tile_rects),(uint8*)(p_tile_rects+i),sizeof(rect_st));
			num_tile_rects++;
		}
	}
	dec_st.num_tile_rects = num_tile_rects;

	/* create clip window list for tile rects */
	num_rects_max = message->num_rects*dec_st.num_tile_rects;
	dec_st.p_tile_rects = (rfx_cvt_rect*) xmalloc(num_rects_max * sizeof(rfx_cvt_rect));
	p_rects_dst = dec_st.p_tile_rects;
	p_rects_ext = dec_st.p_tile_rects + dec_st.num_tile_rects;	/* for extra rects when 1 tile belongs to more than 1 rects. */
	num_tile_rects = 0;
	for (i = 0; i < dec_st.num_tile_rects; i++)
	{
		num_match = rfx_cvt_get_clip(message, p_tile_rects, p_rects_dst, p_rects_ext);
		if (!num_match)
		{
			printf("interesting ???\n");
			p_rects_dst->clip.x      = p_tile_rects->x ;
			p_rects_dst->clip.y      = p_tile_rects->y ;
			p_rects_dst->clip.width  = p_tile_rects->width ;
			p_rects_dst->clip.height = p_tile_rects->height ;
			p_rects_dst++;
			num_tile_rects++;
		}
		else
		{
			p_rects_dst++;
			p_rects_ext+=(num_match-1);
		}
		p_tile_rects++;
		num_tile_rects += num_match;
	}
	p_tile_rects-=dec_st.num_tile_rects;
	xfree(p_tile_rects);
	dec_st.num_tile_rects = num_tile_rects;

	if (dec_st.num_tile_rects > 160)
		printf("******Error : too many rectangles ( %d )******\n",dec_st.num_tile_rects);

	g_idx[disp_idx] = 1 - idx;

	//Call driver
	io_err = ioctl(priv->fd, DEV_IOCTL_DECODING, &dec_st);
	if (io_err)
		printf("Hardware decoding failed.\n");

	xfree(dec_st.p_size);
	xfree(dec_st.p_tile_rects);

}

static void rfx_process_message_sync(RFX_CONTEXT* context, STREAM* s)
{
	uint32 magic;

	rfx_parse_message_sync(s, &magic, &context->version);

	if (magic != WF_MAGIC)
	{
		DEBUG_WARN("invalid magic number 0x%X", magic);
		return;
	}

	if (context->version != WF_VERSION_1_0)
	{
		DEBUG_WARN("unknown version number 0x%X", context->version);
		return;
	}

	DEBUG_RFX("version 0x%X", context->version);
}

static void rfx_process_message_codec_versions(RFX_CONTEXT* context, STREAM* s)
{
	uint8 numCodecs;

	rfx_parse_message_codec_versions(s, &numCodecs, &context->codec_id, &context->codec_version);

	if (numCodecs != 1)
	{
		DEBUG_WARN("numCodecs: %d, expected:1", numCodecs);
		return;
	}

	DEBUG_RFX("id %d version 0x%X.", context->codec_id, context->codec_version);
}

static void rfx_process_message_channels(RFX_CONTEXT* context, STREAM* s)
{
	context->channels = rfx_parse_message_channels(s, &context->num_channels, context->channels);
	/* In RDVH sessions, numChannels will represent the number of virtual monitors
	 * configured and does not always be set to 0x01 as [MS-RDPRFX] said.
	 */
	if (context->num_channels < 1)
	{
		DEBUG_WARN("numChannels:%d, expected:1", context->num_channels);
		return;
	}

	/* Now, only the first monitor can be used, therefore the other channels will be ignored. */
	context->width = context->channels[0].width;
	context->height = context->channels[0].height;

	DEBUG_RFX("numChannels %d id %d, %dx%d",
		context->num_channels, context->channels[0].channelId, context->width, context->height);
}

static void rfx_process_message_context(RFX_CONTEXT* context, STREAM* s)
{
	uint8 ctxId;
	uint16 tileSize;
	uint16 properties;

	rfx_parse_message_context(s, &ctxId, &tileSize, &properties);

	DEBUG_RFX("ctxId %d tileSize %d properties 0x%X.", ctxId, tileSize, properties);

	context->flags = (properties & 0x0007);

	if (context->flags == CODEC_MODE)
		DEBUG_RFX("codec is in image mode.");
	else
		DEBUG_RFX("codec is in video mode.");

	switch ((properties & 0x1E00) >> 9)
	{
		case CLW_ENTROPY_RLGR1:
			context->mode = RLGR1;
			DEBUG_RFX("RLGR1.");
			break;

		case CLW_ENTROPY_RLGR3:
			context->mode = RLGR3;
			DEBUG_RFX("RLGR3.");
			break;

		default:
			DEBUG_WARN("unknown RLGR algorithm.");
			break;
	}
}

static void rfx_process_message_frame_begin(RFX_CONTEXT* context, RFX_MESSAGE* message, STREAM* s)
{
	uint32 frameIdx;
	uint16 numRegions;

	rfx_parse_message_frame_begin(s, &frameIdx, &numRegions);

	DEBUG_RFX("RFX_FRAME_BEGIN: frameIdx:%d numRegions:%d", frameIdx, numRegions);
}

static void rfx_process_message_frame_end(RFX_CONTEXT* context, RFX_MESSAGE* message, STREAM* s)
{
}

static void rfx_process_message_region(RFX_CONTEXT* context, RFX_MESSAGE* message, STREAM* s)
{
	uint8 regionFlags;
	message->rects = rfx_parse_message_region(s, &regionFlags, &message->num_rects, message->rects);
}

RFX_MESSAGE* rfx_process_message(RFX_CONTEXT* context, uint8* data, uint32 length, RECTANGLE_16* dst_rect)
{
	int pos;
	STREAM* s;
	uint32 blockLen;
	uint32 blockType;
	RFX_CONTEXT_CVT_PRIV* priv;
	RFX_MESSAGE* message;

	s = stream_new(0);

	message = rfx_message_new();
	if (dst_rect)
		memcpy(&message->dst_rect, dst_rect, sizeof(RECTANGLE_16));

	stream_attach(s, data, length);

	while (stream_get_left(s) > 6)
	{
		/* RFX_BLOCKT */
		stream_read_uint16(s, blockType); /* blockType (2 bytes) */
		stream_read_uint32(s, blockLen); /* blockLen (4 bytes) */

		DEBUG_RFX("blockType 0x%X blockLen %d", blockType, blockLen);

		if (blockLen == 0)
		{
			DEBUG_WARN("zero blockLen");
			break;
		}

		pos = stream_get_pos(s) - 6 + blockLen;

		if (blockType >= WBT_CONTEXT && blockType <= WBT_EXTENSION)
		{
			/* RFX_CODEC_CHANNELT */
			/* codecId (1 byte) must be set to 0x01 */
			/* channelId (1 byte) must be set to 0x00 */
			stream_seek(s, 2);
		}

		switch (blockType)
		{
			case WBT_SYNC:
				rfx_process_message_sync(context, s);
				break;

			case WBT_CODEC_VERSIONS:
				rfx_process_message_codec_versions(context, s);
				break;

			case WBT_CHANNELS:
				rfx_process_message_channels(context, s);
				break;

			case WBT_CONTEXT:
				rfx_process_message_context(context, s);
				break;

			case WBT_FRAME_BEGIN:
				rfx_process_message_frame_begin(context, message, s);
				break;

			case WBT_FRAME_END:
				rfx_process_message_frame_end(context, message, s);
				break;

			case WBT_REGION:
				rfx_process_message_region(context, message, s);
				break;

			case WBT_EXTENSION:
				priv = (RFX_CONTEXT_CVT_PRIV*) context->priv;
				if (priv->rfx_op_mode == RDVH_OPT)
					rfx_cvt_rdvh_opt_process_message_tileset(context, message, s);
				else
					rfx_cvt_process_message_tileset(context, message, s);
				break;

			default:
				DEBUG_WARN("unknown blockType 0x%X", blockType);
				break;
		}

		stream_set_pos(s, pos);
	}

	stream_detach(s);
	stream_free(s);

	return message;
}


static void rfx_context_set_pixel_format(void* context, RDP_PIXEL_FORMAT pixel_format)
{
	RFX_CONTEXT* rfx_context = (RFX_CONTEXT*) context;

	rfx_context->pixel_format = pixel_format;
    rfx_context->bits_per_pixel = rfx_get_bits_per_pixel(pixel_format);
}

/****************************
 * Public APIs              *
 ****************************/
RFX_CONTEXT* rfx_context_new(void* info)
{
	RFX_CONTEXT* context;
	RFX_CONTEXT_CVT_PRIV* priv;
	rdpSettings* settings;
	rdpCvtSettings* cvt_settings;

	uint8 mon_layout;
	int width;
	int height;

	dec_setup_st d_st;
	resolution_st r_st;
	mon_set_st ms_st;
	int error;
	char frame_buffer_dev[20];	/* hai */
	int fd;

	settings = (rdpSettings*) info;
	cvt_settings = (rdpCvtSettings*) settings->priv;

	context = xnew(RFX_CONTEXT);
	priv = xnew(RFX_CONTEXT_CVT_PRIV);
	context->priv = priv;

	/* register context callbacks */
	context->reset = NULL;
	context->set_cpu_opt = NULL;
	context->set_pixel_format = rfx_context_set_pixel_format;

	context->num_channels = 0;
	context->channels = NULL;

	/* initialize the default pixel format */
	rfx_context_set_pixel_format(context, RDP_PIXEL_FORMAT_B8G8R8A8);

	mon_layout = cvt_settings->monitor_layout;
	width = settings->width;
	height = settings->height;

	priv->fd = -1;
	priv->rfx_op_mode = cvt_settings->opmode;
	priv->rfx_mon_layout = mon_layout;

	printf("RemoteFX Hardware Mode with Cloudvue CT8311\n");
	fd = open("/dev/ct8311", O_RDWR);
	if (fd != -1)
	{
		if (settings->num_monitors == 2)
		{
			if ((mon_layout == MON_LAYOUT_T0_B1) || (mon_layout == MON_LAYOUT_T1_B0))
			{
				r_st.width  = width;
				r_st.height = height >> 1;
			}
			else
			{
				r_st.width  = width >> 1;
				r_st.height = height;
			}
		}
		else
		{
			r_st.width  = width ;
			r_st.height = height;
		}
		r_st.bpp = 32;
		r_st.vxres = width;
		r_st.vyres = height;

		d_st.width = r_st.width;
		d_st.height = r_st.height;
		d_st.op_mode = (cvU32)cvt_settings->opmode;
		ms_st.mon_layout = mon_layout;
		ms_st.mon_rate = 0;

		error = ioctl(fd, DEV_IOCTL_DECODER_SETUP, &d_st);
		if (error < 0)
		{
			printf("DEV_IOCTL_DECODER_SETUP failed.\n");
		}
		error = ioctl(fd, DEV_IOCTL_MONITOR_SET, &ms_st);
		if (error < 0)
		{
			printf("DEV_IOCTL_MONITOR_SET failed.\n");
		}
		error = ioctl(fd, DEV_IOCTL_RESOLUTION_SET, &r_st);
		if (error < 0)
		{
			printf("DEV_IOCTL_RESOLUTION_SET failed.\n");
		}
		priv->fd = fd;
	}
	else
	{
		sprintf(frame_buffer_dev, "/dev/fb%d", cvt_settings->fb_num);
		fd = open(frame_buffer_dev, O_RDWR);
		if (fd == -1)
		{
			printf("Failed to open %s\n",frame_buffer_dev);
			xfree(context);
			return (RFX_CONTEXT*)NULL;
		}

		if (settings->num_monitors == 2)
		{
			if ((mon_layout==MON_LAYOUT_T0_B1)||(mon_layout==MON_LAYOUT_T1_B0))
			{
				d_st.width  = width;
				d_st.height = height >> 1;
			}
			else
			{
				d_st.width  = width >> 1;
				d_st.height = height;
			}
		}
		else
		{
			d_st.width  = width ;
			d_st.height = height;
		}
		d_st.op_mode = (cvU32)cvt_settings->opmode;
		printf("\n=== Resolution: %d x %d on %d monitor(s), mode=%d\n", d_st.width, d_st.height, settings->num_monitors, cvt_settings->opmode);


		error = ioctl(fd, DEV_IOCTL_DECODER_SETUP, &d_st);
		if (error < 0)
		{
			printf("DEV_IOCTL_DECODER_SETUP failed.\n");
		}
		priv->fd = fd;
	}

	return context;
}

void rfx_context_free(RFX_CONTEXT* context)
{
	int io_err;
	RFX_CONTEXT_CVT_PRIV* priv = (RFX_CONTEXT_CVT_PRIV*)context->priv;

	printf("Decoder cleanup...\n");
	io_err = ioctl(priv->fd, DEV_IOCTL_DECODER_CLEANUP, NULL);
	if (io_err)
		printf("Decoder cleanup failed.\n");

	if (priv->fd)
		close(priv->fd);

	xfree(context->quants);
	xfree(context->channels);
	xfree(context->priv);
	xfree(context);
}

RFX_MESSAGE* rfx_message_new(void)
{
	RFX_MESSAGE* message;

	message = xnew(RFX_MESSAGE);
	return message;
}

void rfx_message_free(RFX_CONTEXT* context, RFX_MESSAGE* message)
{
	if (message != NULL)
	{
		xfree(message->rects);
		xfree(message);
	}
}

