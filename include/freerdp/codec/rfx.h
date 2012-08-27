/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec
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

#ifndef __RFX_H
#define __RFX_H
#include "config.h"
#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/platform.h>
#include <freerdp/utils/stream.h>

#ifdef __cplusplus
extern "C" {
#endif

enum _RLGR_MODE
{
	RLGR1,
	RLGR3
};
typedef enum _RLGR_MODE RLGR_MODE;

struct _RFX_RECT
{
	uint16 x;
	uint16 y;
	uint16 width;
	uint16 height;
};
typedef struct _RFX_RECT RFX_RECT;

struct _RFX_TILE
{
	uint16 x;
	uint16 y;
	uint8* data;
};
typedef struct _RFX_TILE RFX_TILE;

struct _RFX_CHANNELT
{
	uint8 channelId;
	uint16 width;
	uint16 height;
};
typedef struct _RFX_CHANNELT RFX_CHANNELT;

struct _RFX_MESSAGE
{
	/**
	 * The rects array represents the updated region of the frame. The UI
	 * requires to clip drawing destination base on the union of the rects.
	 */
	uint16 num_rects;
	RFX_RECT* rects;

	/**
	 * The tiles array represents the actual frame data. Each tile is always
	 * 64x64. Note that only pixels inside the updated region (represented as
	 * rects described above) are valid. Pixels outside of the region may
	 * contain arbitrary data.
	 */
	uint16 num_tiles;
	RFX_TILE** tiles;

	/** 
	 * Destination Rectangle, filled by TS_SURFCMD_SET_SURF_BITS or 
	 * TS_SURFCMD_STREAM_SURF_BITS command. All fields in this structure
	 * need to be zero when decoder be used to decode bitmap cache v3
	 */
	 RECTANGLE_16 dst_rect;
};
typedef struct _RFX_MESSAGE RFX_MESSAGE;

/**
 * Callback function prototypes for RemoteFX context 
 */
typedef void (*pRfxContextSetPixelFormat)(void* context, RDP_PIXEL_FORMAT pixel_format);
typedef void (*pRfxContextSetCpuOpt)(void* context, uint32 cpu_opt);
typedef void (*pRfxContextReset)(void* context);

typedef struct _RFX_COMPOSE_CONTEXT_PRIV RFX_COMPOSE_CONTEXT_PRIV;
struct _RFX_COMPOSE_CONTEXT
{
	uint16 flags;
	uint16 width;
	uint16 height;
	uint8 num_channels;
	RFX_CHANNELT* channels;
	RLGR_MODE mode;
	uint16 properties;
	uint32 version;
	uint8 codec_id;
	uint16 codec_version;
	RDP_PIXEL_FORMAT pixel_format;
	uint8 bits_per_pixel;

	/* color palette allocated by the application */
	const uint8* palette;

	/* temporary data within a frame */
	uint32 frame_idx;
	boolean header_processed;
	uint8 num_quants;
	uint32* quants;
	uint8 quant_idx_y;
	uint8 quant_idx_cb;
	uint8 quant_idx_cr;

	pRfxContextSetPixelFormat set_pixel_format;
	pRfxContextSetCpuOpt set_cpu_opt;
	pRfxContextReset reset;
	/* private definitions for encoder */
	void* priv;
};
typedef struct _RFX_COMPOSE_CONTEXT RFX_COMPOSE_CONTEXT;

struct _RFX_CONTEXT
{
	uint16 flags;
	uint16 width;
	uint16 height;
	uint8 num_channels;
	RFX_CHANNELT* channels;
	RLGR_MODE mode;
	uint32 version;
	uint8 codec_id;
	uint16 codec_version;
	RDP_PIXEL_FORMAT pixel_format;
	uint8 bits_per_pixel;

	/* temporary data within a frame */
	uint8 num_quants;
	uint32* quants;

	pRfxContextSetPixelFormat set_pixel_format;
	pRfxContextSetCpuOpt set_cpu_opt;
	pRfxContextReset reset;

	/* private definitions for decoder */
	void* priv;
};
typedef struct _RFX_CONTEXT RFX_CONTEXT;

/**
 * Common entry points for all platforms
 */
FREERDP_API RFX_CONTEXT* rfx_context_new(void* info);
FREERDP_API void rfx_context_free(RFX_CONTEXT* context);

FREERDP_API RFX_MESSAGE* rfx_message_new(void);
FREERDP_API void rfx_message_free(RFX_CONTEXT* context, RFX_MESSAGE* message);

FREERDP_API RFX_COMPOSE_CONTEXT* rfx_compose_context_new(void);
FREERDP_API void rfx_compose_context_free(RFX_COMPOSE_CONTEXT* context);

FREERDP_API void rfx_compose_message_header(RFX_COMPOSE_CONTEXT* context, STREAM* s);
FREERDP_API void rfx_compose_message(RFX_COMPOSE_CONTEXT* context, STREAM* s,
	const RFX_RECT* rects, int num_rects, uint8* image_data, int width, int height, int rowstride);

FREERDP_API RFX_MESSAGE* rfx_process_message(RFX_CONTEXT* context, uint8* data, uint32 length, RECTANGLE_16* dst_rect);

#include PLATFORM_GLOBAL_HEADER_FILE(freerdp/codec/rfx, FREERDP_PLATFORM)

#ifdef __cplusplus
}
#endif


#endif /* __RFX_H */
