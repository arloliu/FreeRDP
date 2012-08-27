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

#ifndef __RFX_UTILS_H
#define __RFX_UTILS_H

#include <freerdp/codec/rfx.h>
#include <freerdp/utils/stream.h>

uint8 rfx_get_bits_per_pixel(RDP_PIXEL_FORMAT pixel_format);

/*
 * RemoteFX message parser utilities for all platforms 
 */
void rfx_parse_message_sync(STREAM* s, uint32* magic, uint32* version);
void rfx_parse_message_codec_versions(STREAM* s, uint8* numCodecs, uint8* codec_id, uint16* codec_version);
RFX_CHANNELT* rfx_parse_message_channels(STREAM* s, uint8* numChannels, RFX_CHANNELT* channels);
void rfx_parse_message_context(STREAM* s, uint8* ctxId, uint16* tileSize, uint16* properties);
void rfx_parse_message_frame_begin(STREAM* s, uint32* frameIdx, uint16* numRegions);
RFX_RECT* rfx_parse_message_region(STREAM* s, uint8* regionFlags, uint16* numRects, RFX_RECT* rects);
void rfx_parse_message_tile(STREAM* s,
	uint8* quantIdxY, uint8* quantIdxCb, uint8* quantIdxCr,
	uint16* xIdx, uint16* yIdx,
	uint16* YLen, uint16* CbLen, uint16* CrLen);
uint32* rfx_parse_message_tileset(STREAM* s, uint8* numQuants, 
	uint16* numTiles, uint32* tilesDataSize, uint32* quants);

#endif /* __RFX_UTILS_H */
