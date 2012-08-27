/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library
 *
 * Copyright 2011 Vic Lee
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

#include <freerdp/codec/rfx.h>
#include <freerdp/utils/memory.h>
#include <freerdp/constants.h>
#include "rfx_constants.h"
#include "rfx_types.h"
#include "rfx_quantization.h"
#include "rfx_dwt.h"
#include "rfx_encode.h"
#include "rfx_compose.h"

#ifdef WITH_SSE2
#include "rfx_sse2.h"
#endif

#ifndef RFX_INIT_ENCODER_SIMD
#define RFX_INIT_ENCODER_SIMD(_rfx_context) do { } while (0)
#endif

static void rfx_compose_profiler_create(RFX_COMPOSE_CONTEXT* context)
{
	RFX_COMPOSE_CONTEXT_PRIV* priv = (RFX_COMPOSE_CONTEXT_PRIV*)priv;

	PROFILER_CREATE(priv->prof_rfx_encode_rgb, "rfx_encode_rgb");
	PROFILER_CREATE(priv->prof_rfx_encode_component, "rfx_encode_component");
	PROFILER_CREATE(priv->prof_rfx_rlgr_encode, "rfx_rlgr_encode");
	PROFILER_CREATE(priv->prof_rfx_differential_encode, "rfx_differential_encode");
	PROFILER_CREATE(priv->prof_rfx_quantization_encode, "rfx_quantization_encode");
	PROFILER_CREATE(priv->prof_rfx_dwt_2d_encode, "rfx_dwt_2d_encode");
	PROFILER_CREATE(priv->prof_rfx_encode_rgb_to_ycbcr, "rfx_encode_rgb_to_ycbcr");
	PROFILER_CREATE(priv->prof_rfx_encode_format_rgb, "rfx_encode_format_rgb");
}

static void rfx_compose_profiler_free(RFX_COMPOSE_CONTEXT* context)
{
	RFX_COMPOSE_CONTEXT_PRIV* priv = (RFX_COMPOSE_CONTEXT_PRIV*)priv;

	PROFILER_FREE(priv->prof_rfx_encode_rgb);
	PROFILER_FREE(priv->prof_rfx_encode_component);
	PROFILER_FREE(priv->prof_rfx_rlgr_encode);
	PROFILER_FREE(priv->prof_rfx_differential_encode);
	PROFILER_FREE(priv->prof_rfx_quantization_encode);
	PROFILER_FREE(priv->prof_rfx_dwt_2d_encode);
	PROFILER_FREE(priv->prof_rfx_encode_rgb_to_ycbcr);
	PROFILER_FREE(priv->prof_rfx_encode_format_rgb);
}

static void rfx_compose_profiler_print(RFX_COMPOSE_CONTEXT* context)
{
	RFX_COMPOSE_CONTEXT_PRIV* priv = (RFX_COMPOSE_CONTEXT_PRIV*)priv;
	PROFILER_PRINT_HEADER;

	PROFILER_PRINT(priv->prof_rfx_decode_rgb);
	PROFILER_PRINT(priv->prof_rfx_decode_component);
	PROFILER_PRINT(priv->prof_rfx_rlgr_decode);
	PROFILER_PRINT(priv->prof_rfx_differential_decode);
	PROFILER_PRINT(priv->prof_rfx_quantization_decode);
	PROFILER_PRINT(priv->prof_rfx_dwt_2d_decode);
	PROFILER_PRINT(priv->prof_rfx_decode_ycbcr_to_rgb);
	PROFILER_PRINT(priv->prof_rfx_decode_format_rgb);

	PROFILER_PRINT(priv->prof_rfx_encode_rgb);
	PROFILER_PRINT(priv->prof_rfx_encode_component);
	PROFILER_PRINT(priv->prof_rfx_rlgr_encode);
	PROFILER_PRINT(priv->prof_rfx_differential_encode);
	PROFILER_PRINT(priv->prof_rfx_quantization_encode);
	PROFILER_PRINT(priv->prof_rfx_dwt_2d_encode);
	PROFILER_PRINT(priv->prof_rfx_encode_rgb_to_ycbcr);
	PROFILER_PRINT(priv->prof_rfx_encode_format_rgb);

	PROFILER_PRINT_FOOTER;
}

static void rfx_compose_context_set_pixel_format(void* context, RDP_PIXEL_FORMAT pixel_format)
{
	RFX_COMPOSE_CONTEXT* rfx_context = (RFX_COMPOSE_CONTEXT*) context;

	rfx_context->pixel_format = pixel_format;
	switch (pixel_format)
	{
		case RDP_PIXEL_FORMAT_B8G8R8A8:
		case RDP_PIXEL_FORMAT_R8G8B8A8:
			rfx_context->bits_per_pixel = 32;
			break;
		case RDP_PIXEL_FORMAT_B8G8R8:
		case RDP_PIXEL_FORMAT_R8G8B8:
			rfx_context->bits_per_pixel = 24;
			break;
		case RDP_PIXEL_FORMAT_B5G6R5_LE:
		case RDP_PIXEL_FORMAT_R5G6B5_LE:
			rfx_context->bits_per_pixel = 16;
			break;
		case RDP_PIXEL_FORMAT_P4_PLANER:
			rfx_context->bits_per_pixel = 4;
			break;
		case RDP_PIXEL_FORMAT_P8:
			rfx_context->bits_per_pixel = 8;
			break;
		default:
			rfx_context->bits_per_pixel = 0;
			break;
	}
}

static void rfx_compose_context_reset(void* context)
{
	RFX_COMPOSE_CONTEXT* rfx_context = (RFX_COMPOSE_CONTEXT*) context;
	rfx_context->header_processed = false;
	rfx_context->frame_idx = 0;
}

static void rfx_compose_context_set_cpu_opt(void* context, uint32 cpu_opt)
{
	/* enable SIMD CPU acceleration if detected */
	if (cpu_opt & CPU_SSE2)
		RFX_INIT_ENCODER_SIMD((RFX_COMPOSE_CONTEXT*) context);
}

/**
 * The quantization values control the compression rate and quality. The value
 * range is between 6 and 15. The higher value, the higher compression rate
 * and lower quality.
 *
 * This is the default values being use by the MS RDP server, and we will also
 * use it as our default values for the encoder. It can be overrided by setting
 * the context->num_quants and context->quants member.
 *
 * The order of the values are:
 * LL3, LH3, HL3, HH3, LH2, HL2, HH2, LH1, HL1, HH1
 */
static const uint32 rfx_default_quantization_values[] =
{
	6, 6, 6, 6, 7, 7, 8, 8, 8, 9
};

static void rfx_compose_message_sync(RFX_COMPOSE_CONTEXT* context, STREAM* s)
{
	stream_write_uint16(s, WBT_SYNC); /* BlockT.blockType */
	stream_write_uint32(s, 12); /* BlockT.blockLen */
	stream_write_uint32(s, WF_MAGIC); /* magic */
	stream_write_uint16(s, WF_VERSION_1_0); /* version */
}

static void rfx_compose_message_codec_versions(RFX_COMPOSE_CONTEXT* context, STREAM* s)
{
	stream_write_uint16(s, WBT_CODEC_VERSIONS); /* BlockT.blockType */
	stream_write_uint32(s, 10); /* BlockT.blockLen */
	stream_write_uint8(s, 1); /* numCodecs */
	stream_write_uint8(s, 1); /* codecs.codecId */
	stream_write_uint16(s, WF_VERSION_1_0); /* codecs.version */
}

static void rfx_compose_message_channels(RFX_COMPOSE_CONTEXT* context, STREAM* s)
{
	stream_write_uint16(s, WBT_CHANNELS); /* BlockT.blockType */
	stream_write_uint32(s, 12); /* BlockT.blockLen */
	stream_write_uint8(s, 1); /* numChannels */
	stream_write_uint8(s, 0); /* Channel.channelId */
	stream_write_uint16(s, context->width); /* Channel.width */
	stream_write_uint16(s, context->height); /* Channel.height */
}

static void rfx_compose_message_context(RFX_COMPOSE_CONTEXT* context, STREAM* s)
{
	uint16 properties;

	stream_write_uint16(s, WBT_CONTEXT); /* CodecChannelT.blockType */
	stream_write_uint32(s, 13); /* CodecChannelT.blockLen */
	stream_write_uint8(s, 1); /* CodecChannelT.codecId */
	stream_write_uint8(s, 0); /* CodecChannelT.channelId */
	stream_write_uint8(s, 0); /* ctxId */
	stream_write_uint16(s, CT_TILE_64x64); /* tileSize */

	/* properties */
	properties = context->flags; /* flags */
	properties |= (COL_CONV_ICT << 3); /* cct */
	properties |= (CLW_XFORM_DWT_53_A << 5); /* xft */
	properties |= ((context->mode == RLGR1 ? CLW_ENTROPY_RLGR1 : CLW_ENTROPY_RLGR3) << 9); /* et */
	properties |= (SCALAR_QUANTIZATION << 13); /* qt */
	stream_write_uint16(s, properties);

	/* properties in tilesets: note that this has different format from the one in TS_RFX_CONTEXT */
	properties = 1; /* lt */
	properties |= (context->flags << 1); /* flags */
	properties |= (COL_CONV_ICT << 4); /* cct */
	properties |= (CLW_XFORM_DWT_53_A << 6); /* xft */
	properties |= ((context->mode == RLGR1 ? CLW_ENTROPY_RLGR1 : CLW_ENTROPY_RLGR3) << 10); /* et */
	properties |= (SCALAR_QUANTIZATION << 14); /* qt */
	context->properties = properties;
}

static void rfx_compose_message_frame_begin(RFX_COMPOSE_CONTEXT* context, STREAM* s)
{
	stream_check_size(s, 14);

	stream_write_uint16(s, WBT_FRAME_BEGIN); /* CodecChannelT.blockType */
	stream_write_uint32(s, 14); /* CodecChannelT.blockLen */
	stream_write_uint8(s, 1); /* CodecChannelT.codecId */
	stream_write_uint8(s, 0); /* CodecChannelT.channelId */
	stream_write_uint32(s, context->frame_idx); /* frameIdx */
	stream_write_uint16(s, 1); /* numRegions */

	context->frame_idx++;
}

static void rfx_compose_message_region(RFX_COMPOSE_CONTEXT* context, STREAM* s,
	const RFX_RECT* rects, int num_rects)
{
	int size;
	int i;

	size = 15 + num_rects * 8;
	stream_check_size(s, size);

	stream_write_uint16(s, WBT_REGION); /* CodecChannelT.blockType */
	stream_write_uint32(s, size); /* set CodecChannelT.blockLen later */
	stream_write_uint8(s, 1); /* CodecChannelT.codecId */
	stream_write_uint8(s, 0); /* CodecChannelT.channelId */
	stream_write_uint8(s, 1); /* regionFlags */
	stream_write_uint16(s, num_rects); /* numRects */

	for (i = 0; i < num_rects; i++)
	{
		stream_write_uint16(s, rects[i].x);
		stream_write_uint16(s, rects[i].y);
		stream_write_uint16(s, rects[i].width);
		stream_write_uint16(s, rects[i].height);
	}

	stream_write_uint16(s, CBT_REGION); /* regionType */
	stream_write_uint16(s, 1); /* numTilesets */
}

static void rfx_compose_message_tile(RFX_COMPOSE_CONTEXT* context, STREAM* s,
	uint8* tile_data, int tile_width, int tile_height, int rowstride,
	const uint32* quantVals, int quantIdxY, int quantIdxCb, int quantIdxCr,
	int xIdx, int yIdx)
{
	int YLen = 0;
	int CbLen = 0;
	int CrLen = 0;
	int start_pos, end_pos;

	stream_check_size(s, 19);
	start_pos = stream_get_pos(s);

	stream_write_uint16(s, CBT_TILE); /* BlockT.blockType */
	stream_seek_uint32(s); /* set BlockT.blockLen later */
	stream_write_uint8(s, quantIdxY);
	stream_write_uint8(s, quantIdxCb);
	stream_write_uint8(s, quantIdxCr);
	stream_write_uint16(s, xIdx);
	stream_write_uint16(s, yIdx);

	stream_seek(s, 6); /* YLen, CbLen, CrLen */

	rfx_encode_rgb(context, tile_data, tile_width, tile_height, rowstride,
		quantVals + quantIdxY * 10, quantVals + quantIdxCb * 10, quantVals + quantIdxCr * 10,
		s, &YLen, &CbLen, &CrLen);

	DEBUG_RFX("xIdx=%d yIdx=%d width=%d height=%d YLen=%d CbLen=%d CrLen=%d",
		xIdx, yIdx, tile_width, tile_height, YLen, CbLen, CrLen);

	end_pos = stream_get_pos(s);

	stream_set_pos(s, start_pos + 2);
	stream_write_uint32(s, 19 + YLen + CbLen + CrLen); /* BlockT.blockLen */
	stream_set_pos(s, start_pos + 13);
	stream_write_uint16(s, YLen);
	stream_write_uint16(s, CbLen);
	stream_write_uint16(s, CrLen);

	stream_set_pos(s, end_pos);
}

static void rfx_compose_message_tileset(RFX_COMPOSE_CONTEXT* context, STREAM* s,
	uint8* image_data, int width, int height, int rowstride)
{
	int size;
	int start_pos, end_pos;
	int i;
	int numQuants;
	const uint32* quantVals;
	const uint32* quantValsPtr;
	int quantIdxY;
	int quantIdxCb;
	int quantIdxCr;
	int numTiles;
	int numTilesX;
	int numTilesY;
	int xIdx;
	int yIdx;
	int tilesDataSize;

	if (context->num_quants == 0)
	{
		numQuants = 1;
		quantVals = rfx_default_quantization_values;
		quantIdxY = 0;
		quantIdxCb = 0;
		quantIdxCr = 0;
	}
	else
	{
		numQuants = context->num_quants;
		quantVals = context->quants;
		quantIdxY = context->quant_idx_y;
		quantIdxCb = context->quant_idx_cb;
		quantIdxCr = context->quant_idx_cr;
	}

	numTilesX = (width + 63) / 64;
	numTilesY = (height + 63) / 64;
	numTiles = numTilesX * numTilesY;

	size = 22 + numQuants * 5;
	stream_check_size(s, size);
	start_pos = stream_get_pos(s);

	stream_write_uint16(s, WBT_EXTENSION); /* CodecChannelT.blockType */
	stream_seek_uint32(s); /* set CodecChannelT.blockLen later */
	stream_write_uint8(s, 1); /* CodecChannelT.codecId */
	stream_write_uint8(s, 0); /* CodecChannelT.channelId */
	stream_write_uint16(s, CBT_TILESET); /* subtype */
	stream_write_uint16(s, 0); /* idx */
	stream_write_uint16(s, context->properties); /* properties */
	stream_write_uint8(s, numQuants); /* numQuants */
	stream_write_uint8(s, 0x40); /* tileSize */
	stream_write_uint16(s, numTiles); /* numTiles */
	stream_seek_uint32(s); /* set tilesDataSize later */

	quantValsPtr = quantVals;
	for (i = 0; i < numQuants * 5; i++)
	{
		stream_write_uint8(s, quantValsPtr[0] + (quantValsPtr[1] << 4));
		quantValsPtr += 2;
	}

	DEBUG_RFX("width:%d height:%d rowstride:%d", width, height, rowstride);

	end_pos = stream_get_pos(s);
	for (yIdx = 0; yIdx < numTilesY; yIdx++)
	{
		for (xIdx = 0; xIdx < numTilesX; xIdx++)
		{
			rfx_compose_message_tile(context, s,
				image_data + yIdx * 64 * rowstride + xIdx * 8 * context->bits_per_pixel,
				(xIdx < numTilesX - 1) ? 64 : width - xIdx * 64,
				(yIdx < numTilesY - 1) ? 64 : height - yIdx * 64,
				rowstride, quantVals, quantIdxY, quantIdxCb, quantIdxCr, xIdx, yIdx);
		}
	}
	tilesDataSize = stream_get_pos(s) - end_pos;
	size += tilesDataSize;
	end_pos = stream_get_pos(s);

	stream_set_pos(s, start_pos + 2);
	stream_write_uint32(s, size); /* CodecChannelT.blockLen */
	stream_set_pos(s, start_pos + 18);
	stream_write_uint32(s, tilesDataSize);

	stream_set_pos(s, end_pos);
}

static void rfx_compose_message_frame_end(RFX_COMPOSE_CONTEXT* context, STREAM* s)
{
	stream_check_size(s, 8);

	stream_write_uint16(s, WBT_FRAME_END); /* CodecChannelT.blockType */
	stream_write_uint32(s, 8); /* CodecChannelT.blockLen */
	stream_write_uint8(s, 1); /* CodecChannelT.codecId */
	stream_write_uint8(s, 0); /* CodecChannelT.channelId */
}

static void rfx_compose_message_data(RFX_COMPOSE_CONTEXT* context, STREAM* s,
	const RFX_RECT* rects, int num_rects, uint8* image_data, int width, int height, int rowstride)
{
	rfx_compose_message_frame_begin(context, s);
	rfx_compose_message_region(context, s, rects, num_rects);
	rfx_compose_message_tileset(context, s, image_data, width, height, rowstride);
	rfx_compose_message_frame_end(context, s);
}


void rfx_compose_message_header(RFX_COMPOSE_CONTEXT* context, STREAM* s)
{
	stream_check_size(s, 12 + 10 + 12 + 13);

	rfx_compose_message_sync(context, s);
	rfx_compose_message_context(context, s);
	rfx_compose_message_codec_versions(context, s);
	rfx_compose_message_channels(context, s);

	context->header_processed = true;
}

void rfx_compose_message(RFX_COMPOSE_CONTEXT* context, STREAM* s,
	const RFX_RECT* rects, int num_rects, uint8* image_data, int width, int height, int rowstride)
{
	/* Only the first frame should send the RemoteFX header */
	if (context->frame_idx == 0 && !context->header_processed)
		rfx_compose_message_header(context, s);

	rfx_compose_message_data(context, s, rects, num_rects, image_data, width, height, rowstride);
}

RFX_COMPOSE_CONTEXT* rfx_compose_context_new(void)
{
	RFX_COMPOSE_CONTEXT* context;
	RFX_COMPOSE_CONTEXT_PRIV* priv;

	context = xnew(RFX_COMPOSE_CONTEXT);
	priv = xnew(RFX_COMPOSE_CONTEXT_PRIV);
	context->priv = priv;

	/* register context callbacks */
	context->reset = rfx_compose_context_reset;
	context->set_cpu_opt = rfx_compose_context_set_cpu_opt;
	context->set_pixel_format = rfx_compose_context_set_pixel_format;

	context->num_channels = 0;
	context->channels = NULL;

	/* initialize the default pixel format */
	rfx_compose_context_set_pixel_format(context, RDP_PIXEL_FORMAT_B8G8R8A8);

	/* align buffers to 16 byte boundary (needed for SSE/SSE2 instructions) */
	priv->y_r_buffer = (sint16*)(((uintptr_t)priv->y_r_mem + 16) & ~ 0x0F);
	priv->cb_g_buffer = (sint16*)(((uintptr_t)priv->cb_g_mem + 16) & ~ 0x0F);
	priv->cr_b_buffer = (sint16*)(((uintptr_t)priv->cr_b_mem + 16) & ~ 0x0F);

	priv->dwt_buffer = (sint16*)(((uintptr_t)priv->dwt_mem + 16) & ~ 0x0F);

	/* create profilers for default decoding routines */
	rfx_compose_profiler_create(context);
	
	/* set up default private routines */
	priv->encode_rgb_to_ycbcr = rfx_encode_rgb_to_ycbcr;
	priv->quantization_encode = rfx_quantization_encode;	
	priv->dwt_2d_encode = rfx_dwt_2d_encode;

	return context;
}

void rfx_compose_context_free(RFX_COMPOSE_CONTEXT* context)
{
	xfree(context->quants);

	rfx_compose_profiler_print(context);
	rfx_compose_profiler_free(context);

	xfree(context->channels);
	xfree(context->priv);
	xfree(context);
}
