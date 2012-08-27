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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/utils/memory.h>
#include <freerdp/constants.h>
#include <freerdp/platform.h>

#include "rfx_constants.h"
#include "rfx_types.h"
#include "rfx_pool.h"
#include "rfx_decode.h"
#include "rfx_encode.h"
#include "rfx_quantization.h"
#include "rfx_dwt.h"
#include "rfx_utils.h"
#include "rfx_platform_default.h"

#ifdef WITH_SSE2
#include "rfx_sse2.h"
#endif

#ifdef WITH_NEON
#include "rfx_neon.h"
#endif

#ifndef RFX_INIT_DECODER_SIMD
#define RFX_INIT_DECODER_SIMD(_rfx_context) do { } while (0)
#endif

static void rfx_profiler_create(RFX_CONTEXT* context)
{
	RFX_CONTEXT_DEFAULT_PRIV* priv = (RFX_CONTEXT_DEFAULT_PRIV*)priv;

	PROFILER_CREATE(priv->prof_rfx_decode_rgb, "rfx_decode_rgb");
	PROFILER_CREATE(priv->prof_rfx_decode_component, "rfx_decode_component");
	PROFILER_CREATE(priv->prof_rfx_rlgr_decode, "rfx_rlgr_decode");
	PROFILER_CREATE(priv->prof_rfx_differential_decode, "rfx_differential_decode");
	PROFILER_CREATE(priv->prof_rfx_quantization_decode, "rfx_quantization_decode");
	PROFILER_CREATE(priv->prof_rfx_dwt_2d_decode, "rfx_dwt_2d_decode");
	PROFILER_CREATE(priv->prof_rfx_decode_ycbcr_to_rgb, "rfx_decode_ycbcr_to_rgb");
	PROFILER_CREATE(priv->prof_rfx_decode_format_rgb, "rfx_decode_format_rgb");
}

static void rfx_profiler_free(RFX_CONTEXT* context)
{
	RFX_CONTEXT_DEFAULT_PRIV* priv = (RFX_CONTEXT_DEFAULT_PRIV*)priv;
	PROFILER_FREE(priv->prof_rfx_decode_rgb);
	PROFILER_FREE(priv->prof_rfx_decode_component);
	PROFILER_FREE(priv->prof_rfx_rlgr_decode);
	PROFILER_FREE(priv->prof_rfx_differential_decode);
	PROFILER_FREE(priv->prof_rfx_quantization_decode);
	PROFILER_FREE(priv->prof_rfx_dwt_2d_decode);
	PROFILER_FREE(priv->prof_rfx_decode_ycbcr_to_rgb);
	PROFILER_FREE(priv->prof_rfx_decode_format_rgb);
}

static void rfx_profiler_print(RFX_CONTEXT* context)
{
	RFX_CONTEXT_DEFAULT_PRIV* priv = (RFX_CONTEXT_DEFAULT_PRIV*)priv;
	PROFILER_PRINT_HEADER;

	PROFILER_PRINT(priv->prof_rfx_decode_rgb);
	PROFILER_PRINT(priv->prof_rfx_decode_component);
	PROFILER_PRINT(priv->prof_rfx_rlgr_decode);
	PROFILER_PRINT(priv->prof_rfx_differential_decode);
	PROFILER_PRINT(priv->prof_rfx_quantization_decode);
	PROFILER_PRINT(priv->prof_rfx_dwt_2d_decode);
	PROFILER_PRINT(priv->prof_rfx_decode_ycbcr_to_rgb);
	PROFILER_PRINT(priv->prof_rfx_decode_format_rgb);

	PROFILER_PRINT_FOOTER;
}

static void rfx_context_set_pixel_format(void* context, RDP_PIXEL_FORMAT pixel_format)
{
	RFX_CONTEXT* rfx_context = (RFX_CONTEXT*) context;

	rfx_context->pixel_format = pixel_format;
    rfx_context->bits_per_pixel = rfx_get_bits_per_pixel(pixel_format);
}

static void rfx_context_reset(void* context)
{
}

static void rfx_context_set_cpu_opt(void* context, uint32 cpu_opt)
{
	RFX_CONTEXT* rfx_context = (RFX_CONTEXT*) context;
	/* enable SIMD CPU acceleration if detected */
	if ( cpu_opt & CPU_SSE2 || cpu_opt & CPU_NEON)
		RFX_INIT_DECODER_SIMD(rfx_context);
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
	DEBUG_RFX("RFX_FRAME_END");
}

static void rfx_process_message_region(RFX_CONTEXT* context, RFX_MESSAGE* message, STREAM* s)
{
	uint8 regionFlags;
	message->rects = rfx_parse_message_region(s, &regionFlags, &message->num_rects, message->rects);
}

static void rfx_process_message_tile(RFX_CONTEXT* context, RFX_TILE* tile, STREAM* s)
{
	uint8 quantIdxY;
	uint8 quantIdxCb;
	uint8 quantIdxCr;
	uint16 xIdx, yIdx;
	uint16 YLen, CbLen, CrLen;

	rfx_parse_message_tile(s,
		&quantIdxY, &quantIdxCb, &quantIdxCr,
		&xIdx, &yIdx, 
		&YLen, &CbLen, &CrLen);

	DEBUG_RFX("quantIdxY:%d quantIdxCb:%d quantIdxCr:%d xIdx:%d yIdx:%d YLen:%d CbLen:%d CrLen:%d",
		quantIdxY, quantIdxCb, quantIdxCr, xIdx, yIdx, YLen, CbLen, CrLen);

	tile->x = xIdx * 64;
	tile->y = yIdx * 64;

	rfx_decode_rgb(context, s,
		YLen, context->quants + (quantIdxY * 10),
		CbLen, context->quants + (quantIdxCb * 10),
		CrLen, context->quants + (quantIdxCr * 10),
		tile->data);
}

static void rfx_process_message_tileset(RFX_CONTEXT* context, RFX_MESSAGE* message, STREAM* s)
{
	int i;
	uint32 blockLen;
	uint32 blockType;
	uint32 tilesDataSize;
	int pos;
	RFX_CONTEXT_DEFAULT_PRIV* priv = (RFX_CONTEXT_DEFAULT_PRIV*)context->priv;

	context->quants = rfx_parse_message_tileset(s, &context->num_quants,
		&message->num_tiles, &tilesDataSize, context->quants);

	message->tiles = rfx_pool_get_tiles(priv->pool, message->num_tiles);

	/* tiles */
	for (i = 0; i < message->num_tiles; i++)
	{
		/* RFX_TILE */
		stream_read_uint16(s, blockType); /* blockType (2 bytes), must be set to CBT_TILE (0xCAC3) */
		stream_read_uint32(s, blockLen); /* blockLen (4 bytes) */

		pos = stream_get_pos(s) - 6 + blockLen;

		if (blockType != CBT_TILE)
		{
			DEBUG_WARN("unknown block type 0x%X, expected CBT_TILE (0xCAC3).", blockType);
			break;
		}

		rfx_process_message_tile(context, message->tiles[i], s);

		stream_set_pos(s, pos);
	}
}

RFX_MESSAGE* rfx_process_message(RFX_CONTEXT* context, uint8* data, uint32 length, RECTANGLE_16* dst_rect)
{
	int pos;
	STREAM* s;
	uint32 blockLen;
	uint32 blockType;
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
				rfx_process_message_tileset(context, message, s);
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



RFX_CONTEXT* rfx_context_new(void* info)
{
	RFX_CONTEXT* context;
	RFX_CONTEXT_DEFAULT_PRIV* priv;

	context = xnew(RFX_CONTEXT);
	priv = xnew(RFX_CONTEXT_DEFAULT_PRIV);
	context->priv = priv;

	/* register context callbacks */
	context->reset = rfx_context_reset;
	context->set_cpu_opt = rfx_context_set_cpu_opt;
	context->set_pixel_format = rfx_context_set_pixel_format;

	priv->pool = rfx_pool_new();

	context->num_channels = 0;
	context->channels = NULL;

	/* initialize the default pixel format */
	rfx_context_set_pixel_format(context, RDP_PIXEL_FORMAT_B8G8R8A8);

	/* align buffers to 16 byte boundary (needed for SSE/SSE2 instructions) */
	priv->y_r_buffer = (sint16*)(((uintptr_t)priv->y_r_mem + 16) & ~ 0x0F);
	priv->cb_g_buffer = (sint16*)(((uintptr_t)priv->cb_g_mem + 16) & ~ 0x0F);
	priv->cr_b_buffer = (sint16*)(((uintptr_t)priv->cr_b_mem + 16) & ~ 0x0F);

	priv->dwt_buffer = (sint16*)(((uintptr_t)priv->dwt_mem + 16) & ~ 0x0F);

	/* create profilers for default decoding routines */
	rfx_profiler_create(context);
	
	/* set up private default routines, mighe be override by SIMD */
	priv->decode_ycbcr_to_rgb = rfx_decode_ycbcr_to_rgb;
	priv->quantization_decode = rfx_quantization_decode;	
	priv->dwt_2d_decode = rfx_dwt_2d_decode;

	return context;
}

void rfx_context_free(RFX_CONTEXT* context)
{
	RFX_CONTEXT_DEFAULT_PRIV* priv = (RFX_CONTEXT_DEFAULT_PRIV*)context->priv;
	xfree(context->quants);

	rfx_pool_free(priv->pool);

	rfx_profiler_print(context);
	rfx_profiler_free(context);

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
    RFX_CONTEXT_DEFAULT_PRIV* priv = (RFX_CONTEXT_DEFAULT_PRIV*)context->priv;

	if (message != NULL)
	{
		xfree(message->rects);

		if (message->tiles != NULL)
		{
			rfx_pool_put_tiles(priv->pool, message->tiles, message->num_tiles);
			xfree(message->tiles);
		}

		xfree(message);
	}
}

