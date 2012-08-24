/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Region Manipulate Utilities
 *
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

#ifndef __REGION_UTILS_H
#define __REGION_UTILS_H
#include <freerdp/api.h>
#include <freerdp/types.h>

struct RDP_BOX 
{
	uint16 x1;
	uint16 x2;
	uint16 y1;
	uint16 y2;
};
typedef struct RDP_BOX rdpBox;

struct RDP_RECT
{
	uint16 x;
	uint16 y;
	uint16 width;
	uint16 height;
};
typedef struct RDP_RECT rdpRect;

struct RDP_REGION
{
	uint32 size;
	uint32 numRects;
	rdpBox* rects;
	rdpBox extents;
};
typedef struct RDP_REGION rdpRegion;

enum RDP_REGION_OP_TYPE 
{
	REGION_OP_UNION = 0,
	REGION_OP_INTERSECT = 1
};
typedef enum RDP_REGION_OP_TYPE rdpRegionOpType;

FREERDP_API rdpRegion* rdp_region_new(void);
FREERDP_API void rdp_region_free(rdpRegion* region);
FREERDP_API boolean rdp_region_init(rdpRegion* region);
FREERDP_API boolean rdp_region_init_rect(rdpRegion* region, rdpRect* rect);
FREERDP_API uint32 rdp_region_length(rdpRegion* region);
FREERDP_API rdpBox* rdp_region_get_rects(rdpRegion* region);
FREERDP_API rdpBox* rdp_region_get_extents(rdpRegion* region);
FREERDP_API boolean rdp_region_union(rdpRegion* region1, rdpRegion* region2, rdpRegion* new_region);
FREERDP_API boolean rdp_region_union_rect(rdpRect* rect, rdpRegion* src, rdpRegion* dst);
FREERDP_API boolean rdp_region_intersect(rdpRegion* region1, rdpRegion* region2, rdpRegion* new_region);
#endif /* __REGION_UTILS_H */
