/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Utils
 *
 * Copyright 2011 Vic Lee
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

#include <freerdp/codec/rfx.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include "rfx_types.h"
#include "rfx_constants.h"
#include "rfx_utils.h"

uint8 rfx_get_bits_per_pixel(RDP_PIXEL_FORMAT pixel_format)
{
    uint8 bpp;
	switch (pixel_format)
	{
		case RDP_PIXEL_FORMAT_B8G8R8A8:
		case RDP_PIXEL_FORMAT_R8G8B8A8:
			bpp = 32;
			break;
		case RDP_PIXEL_FORMAT_B8G8R8:
		case RDP_PIXEL_FORMAT_R8G8B8:
			bpp = 24;
			break;
		case RDP_PIXEL_FORMAT_B5G6R5_LE:
		case RDP_PIXEL_FORMAT_R5G6B5_LE:
			bpp = 16;
			break;
		case RDP_PIXEL_FORMAT_P4_PLANER:
			bpp = 4;
			break;
		case RDP_PIXEL_FORMAT_P8:
			bpp = 8;
			break;
		default:
			bpp = 0;
			break;
	}
    return bpp;
}

void rfx_parse_message_sync(STREAM* s, uint32* magic, uint32* version)
{
	/* RFX_SYNC */
	stream_read_uint32(s, *magic); /* magic (4 bytes), 0xCACCACCA */
	stream_read_uint16(s, *version); /* version (2 bytes), WF_VERSION_1_0 (0x0100) */
}

void rfx_parse_message_codec_versions(STREAM* s, uint8* numCodecs, uint8* codec_id, uint16* codec_version)
{
	stream_read_uint8(s, *numCodecs); /* numCodecs (1 byte), must be set to 0x01 */
	/* RFX_CODEC_VERSIONT */
	stream_read_uint8(s, *codec_id); /* codecId (1 byte) */
	stream_read_uint8(s, *codec_version); /* version (2 bytes) */
}

RFX_CHANNELT* rfx_parse_message_channels(STREAM* s, uint8* numChannels, RFX_CHANNELT* channels)
{
	int i;
	uint8 num_channels;
	stream_read_uint8(s, num_channels); /* numChannels (1 byte), must bet set to 0x01 */

	if (num_channels < 1)
	{
		DEBUG_WARN("numChannels:%d, expected:1", num_channels);
		return channels;
	}
	
	/*
	 * Allocate new RFX_CHANNELT list if empty, and re-allocate new size 
	 * if previous size if smaller than new one
	 */
	if (!channels || *numChannels < 1)
	{
		channels = xmalloc(num_channels * sizeof(RFX_CHANNELT));
	}
	else if (*numChannels < num_channels)
	{
		channels = xrealloc(channels, num_channels * sizeof(RFX_CHANNELT));
	}
	*numChannels = num_channels;

	for(i = 0; i < num_channels; ++i)
	{
		/* RFX_CHANNELT */
		stream_read_uint8(s, channels[i].channelId); /* channelId (1 byte) */
		stream_read_uint16(s, channels[i].width); /* width (2 bytes) */
		stream_read_uint16(s, channels[i].height); /* height (2 bytes) */
	}

	return channels;
}

void rfx_parse_message_context(STREAM* s, uint8* ctxId, uint16* tileSize, uint16* properties)
{
	stream_read_uint8(s, *ctxId); /* ctxId (1 byte), must be set to 0x00 */
	stream_read_uint16(s, *tileSize); /* tileSize (2 bytes), must be set to CT_TILE_64x64 (0x0040) */
	stream_read_uint16(s, *properties); /* properties (2 bytes) */
}

void rfx_parse_message_frame_begin(STREAM* s, uint32* frameIdx, uint16* numRegions)
{
	stream_read_uint32(s, *frameIdx); /* frameIdx (4 bytes), if codec is in video mode, must be ignored */
	stream_read_uint16(s, *numRegions); /* numRegions (2 bytes) */
}

RFX_RECT* rfx_parse_message_region(STREAM* s, uint8* regionFlags, uint16* numRects, RFX_RECT* rects)
{
	int i;
	uint16 num_rects;
	stream_read_uint8(s, *regionFlags);
	stream_read_uint16(s, num_rects); /* numRects (2 bytes) */
	
	if (num_rects < 1)
		return rects;

	if (!rects || *numRects < 1)
	{
		rects = xmalloc(num_rects * sizeof(RFX_RECT));
	}
	else
	{
		rects = xrealloc(rects, num_rects * sizeof(RFX_RECT));
	}
	*numRects = num_rects;
	
	for (i = 0; i < num_rects; ++i)
	{
		/* RFX_RECT */
		stream_read_uint16(s, rects[i].x); /* x (2 bytes) */
		stream_read_uint16(s, rects[i].y); /* y (2 bytes) */
		stream_read_uint16(s, rects[i].width); /* width (2 bytes) */
		stream_read_uint16(s, rects[i].height); /* height (2 bytes) */		
	}
	return rects;
}

void rfx_parse_message_tile(STREAM* s,
	uint8* quantIdxY, uint8* quantIdxCb, uint8* quantIdxCr,
	uint16* xIdx, uint16* yIdx,
	uint16* YLen, uint16* CbLen, uint16* CrLen)
{
	/* RFX_TILE */
	stream_read_uint8(s, *quantIdxY); /* quantIdxY (1 byte) */
	stream_read_uint8(s, *quantIdxCb); /* quantIdxCb (1 byte) */
	stream_read_uint8(s, *quantIdxCr); /* quantIdxCr (1 byte) */
	stream_read_uint16(s, *xIdx); /* xIdx (2 bytes) */
	stream_read_uint16(s, *yIdx); /* yIdx (2 bytes) */
	stream_read_uint16(s, *YLen); /* YLen (2 bytes) */
	stream_read_uint16(s, *CbLen); /* CbLen (2 bytes) */
	stream_read_uint16(s, *CrLen); /* CrLen (2 bytes) */
}

uint32* rfx_parse_message_tileset(STREAM* s, uint8* numQuants, 
	uint16* numTiles, uint32* tilesDataSize, uint32* quants)

{
	int i;
	uint8 quant;
	uint32* quants_ptr;
	uint16 subtype;

	stream_read_uint16(s, subtype); /* subtype (2 bytes) must be set to CBT_TILESET (0xCAC2) */

	if (subtype != CBT_TILESET)
	{
		DEBUG_WARN("invalid subtype, expected CBT_TILESET.");
		return quants;
	}

	stream_seek_uint16(s); /* idx (2 bytes), must be set to 0x0000 */
	stream_seek_uint16(s); /* properties (2 bytes) */

	stream_read_uint8(s, *numQuants); /* numQuant (1 byte) */
	stream_seek_uint8(s); /* tileSize (1 byte), must be set to 0x40 */

	if (*numQuants < 1)
	{
		DEBUG_WARN("no quantization value.");
		return quants;
	}

	stream_read_uint16(s, *numTiles); /* numTiles (2 bytes) */

	if (*numTiles < 1)
	{
		DEBUG_WARN("no tiles.");
		return quants;
	}

	stream_read_uint32(s, *tilesDataSize); /* tilesDataSize (4 bytes) */

	if (quants != NULL)
		quants = (uint32*) xrealloc((void*)quants, *numQuants * 10 * sizeof(uint32));
	else
		quants = (uint32*) xmalloc(*numQuants * 10 * sizeof(uint32));
	quants_ptr = quants;

	/* quantVals */
	for (i = 0; i < *numQuants; i++)
	{
		/* RFX_CODEC_QUANT */
		stream_read_uint8(s, quant);
		*quants_ptr++ = (quant & 0x0F);
		*quants_ptr++ = (quant >> 4);
		stream_read_uint8(s, quant);
		*quants_ptr++ = (quant & 0x0F);
		*quants_ptr++ = (quant >> 4);
		stream_read_uint8(s, quant);
		*quants_ptr++ = (quant & 0x0F);
		*quants_ptr++ = (quant >> 4);
		stream_read_uint8(s, quant);
		*quants_ptr++ = (quant & 0x0F);
		*quants_ptr++ = (quant >> 4);
		stream_read_uint8(s, quant);
		*quants_ptr++ = (quant & 0x0F);
		*quants_ptr++ = (quant >> 4);

		DEBUG_RFX("quant %d (%d %d %d %d %d %d %d %d %d %d).",
			i, quants[i * 10], quants[i * 10 + 1],
			quants[i * 10 + 2], quants[i * 10 + 3],
			quants[i * 10 + 4], quants[i * 10 + 5],
			quants[i * 10 + 6], quants[i * 10 + 7],
			quants[i * 10 + 8], quants[i * 10 + 9]);
	}

	return quants;
}

