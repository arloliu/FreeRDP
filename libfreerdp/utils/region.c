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

#include <string.h>

#include <freerdp/utils/region.h>
#include <freerdp/utils/memory.h>

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef RDP_BOX_SIZE
	#define RDP_BOX_SIZE sizeof(rdpBox)
#endif
#ifndef RDP_BOX_COPY
	#define RDP_BOX_COPY(dst, src) (dst)->x1 = ((rdpBox*)(src))->x1; (dst)->y1 = (src)->y1; (dst)->x2 = (src)->x2; (dst)->y2 = (src)->y2;
#endif

#define EMPTY_REGION(pRegion) pRegion->numRects = 0
#define REGION_NOT_EMPTY(pRegion) pRegion->numRects
#define RDP_REGION_EXTENTS_OVERLAP(r1, r2) \
	((r1)->x2 > (r2)->x1 && \
	 (r1)->x1 < (r2)->x2 && \
	 (r1)->y2 > (r2)->y1 && \
	 (r1)->y1 < (r2)->y2)


typedef int (*pOverlapCb)(rdpRegion* pReg, rdpBox* r1, rdpBox* r1_end, rdpBox* r2, rdpBox* r2_end, sint16  y1, sint16  y2);
typedef int (*pNonOverlapCb)(rdpRegion* pReg, rdpBox* r, rdpBox* rEnd, sint16  y1, sint16  y2);

static int rdp_region_union_overlap(rdpRegion* pReg, rdpBox* r1, rdpBox* r1_end, rdpBox* r2, rdpBox* r2_end, sint16 y1, sint16 y2);
static int rdp_region_union_non_overlap(rdpRegion* pReg, rdpBox* r, rdpBox* rEnd, sint16 y1, sint16 y2);
static int rdp_region_intersect_overlap(rdpRegion* pReg, rdpBox* r1, rdpBox* r1_end, rdpBox* r2, rdpBox* r2_end, sint16 y1, sint16 y2);

struct RDP_REGION_OP_FUNCS
{
	pOverlapCb overlapFunc;
	pNonOverlapCb nonOverlap1Func;
	pNonOverlapCb nonOverlap2Func;
};

static struct RDP_REGION_OP_FUNCS rdp_region_op_funcs[] = {
	{rdp_region_union_overlap, rdp_region_union_non_overlap, rdp_region_union_non_overlap},
	{rdp_region_intersect_overlap, NULL, NULL},
	{NULL, NULL, NULL}
};

static inline rdpBox* rdp_region_check(rdpRegion* region, rdpBox* next_rect)
{
	if (region->numRects >= (region->size - 1))
	{
		region->rects = (rdpBox*)xrealloc(region->rects, (2 * region->size * RDP_BOX_SIZE));
		if (!region->rects)
			return NULL;
		region->size *= 2;
		next_rect = &region->rects[region->numRects];
	}
	return next_rect;
}

static void rdp_region_copy(rdpRegion* dst, rdpRegion* src)
{
	if (dst != src)
	{
		if (dst->size < src->numRects)
		{
			if (dst->rects)
			{
				rdpBox *prev = dst->rects;
				dst->rects = xrealloc(dst->rects, src->numRects * RDP_BOX_SIZE);
				if (!dst->rects)
				{
					xfree(prev);
					return;
				}
			}
			dst->size = src->numRects;
		}
		dst->numRects = src->numRects;
		RDP_BOX_COPY(&dst->extents, &src->extents);
		dst->extents.x1 = src->extents.x1;
		dst->extents.y1 = src->extents.y1;
		dst->extents.x2 = src->extents.x2;
		dst->extents.y2 = src->extents.y2;

		memcpy( (void*)dst->rects, (void*)src->rects, src->numRects * RDP_BOX_SIZE);
	}
}

static inline boolean rdp_region_contains(rdpRegion* region1, rdpRegion* region2)
{
	if ((region1->numRects == 1) &&
		(region1->extents.x1 <= region2->extents.x1) &&
		(region1->extents.y1 <= region2->extents.y1) &&
		(region1->extents.x2 >= region2->extents.x2) &&
		(region1->extents.y2 >= region2->extents.y2))
	{
		return true;
	}
	return false;
}

static int rdp_region_combine(rdpRegion* pReg, int prevStart, int curStart)
{
	rdpBox*	pPrevBox;
	rdpBox*	pCurBox;
	rdpBox*	pRegEnd;
	int curNumRects;
	int prevNumRects;
	int bandY1;

	pRegEnd = &pReg->rects[pReg->numRects];
	pPrevBox = &pReg->rects[prevStart];
	prevNumRects = curStart - prevStart;

	/* Find out how many rectangles are in the current band. */
	pCurBox = &pReg->rects[curStart];
	bandY1 = pCurBox->y1;

	for (curNumRects = 0; (pCurBox != pRegEnd) && (pCurBox->y1 == bandY1); curNumRects++)
	{
		pCurBox++;
	}

	if (pCurBox != pRegEnd)
	{
		pRegEnd--;
		while (pRegEnd[-1].y1 == pRegEnd->y1)
		{
			pRegEnd--;
		}
		curStart = pRegEnd - pReg->rects;
		pRegEnd = pReg->rects + pReg->numRects;
	}

	if ((curNumRects == prevNumRects) && (curNumRects != 0))
	{
		pCurBox -= curNumRects;

		if (pPrevBox->y2 == pCurBox->y1)
		{
			do
			{
				if ((pPrevBox->x1 != pCurBox->x1) || (pPrevBox->x2 != pCurBox->x2))
				{
					return (curStart);
				}
				pPrevBox++;
				pCurBox++;
				prevNumRects -= 1;
			} while (prevNumRects != 0);

			pReg->numRects -= curNumRects;
			pCurBox -= curNumRects;
			pPrevBox -= curNumRects;

			do
			{
				pPrevBox->y2 = pCurBox->y2;
				pPrevBox++;
				pCurBox++;
				curNumRects -= 1;
			} while (curNumRects != 0);

			if (pCurBox == pRegEnd)
			{
				curStart = prevStart;
			}
			else
			{
				do
				{
					*pPrevBox++ = *pCurBox++;
				} while (pCurBox != pRegEnd);
			}
		}
	}
	return (curStart);
}

/*
 * Set region extents according to internal rectangles
 */
static void rdp_region_set_extents(rdpRegion* region)
{
	rdpBox* cur_box, *box_end, *extents;

	if (region->numRects == 0)
	{
		region->extents.x1 = 0;
		region->extents.y1 = 0;
		region->extents.x2 = 0;
		region->extents.y2 = 0;
		return;
	}

	extents = &region->extents;
	cur_box = region->rects;
	box_end = &cur_box[region->numRects - 1];

	extents->x1 = cur_box->x1;
	extents->y1 = cur_box->y1;
	extents->x2 = box_end->x2;
	extents->y2 = box_end->y2;

	while (cur_box <= box_end)
	{
		if (cur_box->x1 < extents->x1)
			extents->x1 = cur_box->x1;

		if (cur_box->x2 > extents->x2)
			extents->x2 = cur_box->x2;
		cur_box++;
	}
}


static inline rdpBox* rdp_region_merge(rdpRegion* reg, rdpBox* cur_rect, rdpBox* next_rect, sint16 y1, sint16 y2)
{
	if ((reg->numRects != 0) &&
		(next_rect[-1].y1 == y1) &&
		(next_rect[-1].y2 == y2) &&
		(next_rect[-1].x2 >= cur_rect->x1))
	{
		if (next_rect[-1].x2 < cur_rect->x2)
		{
			next_rect[-1].x2 = cur_rect->x2;
		}
	}
	else
	{
		next_rect = rdp_region_check(reg, next_rect);
		next_rect->y1 = y1;
		next_rect->y2 = y2;
		next_rect->x1 = cur_rect->x1;
		next_rect->x2 = cur_rect->x2;
		reg->numRects++;
		next_rect++;
	}

	return next_rect;
}

static void rdp_region_op(rdpRegion* new_reg,  rdpRegion* reg1,  rdpRegion* reg2, rdpRegionOpType type)
{
	pOverlapCb overlapFunc = NULL;
	pNonOverlapCb nonOverlap1Func = NULL;
	pNonOverlapCb nonOverlap2Func = NULL;
	rdpBox*	r1;
	rdpBox*	r2;
	rdpBox* r1_end;
	rdpBox* r2_end;
	sint16 ybot;
	sint16 ytop;
	rdpBox* old_rects;
	int prev_band;
	int cur_band;
	rdpBox* r1_band_end;
	rdpBox* r2_band_end;
	sint16 top;
	sint16 bot;

	overlapFunc = rdp_region_op_funcs[type].overlapFunc;
	nonOverlap1Func = rdp_region_op_funcs[type].nonOverlap1Func;
	nonOverlap2Func = rdp_region_op_funcs[type].nonOverlap2Func;

	r1 = reg1->rects;
	r2 = reg2->rects;
	r1_end = r1 + reg1->numRects;
	r2_end = r2 + reg2->numRects;

	old_rects = new_reg->rects;

	EMPTY_REGION(new_reg);

	/* Allocate large enough size to prevent reallocate */
	new_reg->size = MAX(reg1->numRects,reg2->numRects) * 2;

	if ( !(new_reg->rects = (rdpBox*)xmalloc(RDP_BOX_SIZE * new_reg->size)) )
	{
		new_reg->size = 0;
		return;
	}

	if (reg1->extents.y1 < reg2->extents.y1)
		ybot = reg1->extents.y1;
	else
		ybot = reg2->extents.y1;

	prev_band = 0;

	do
	{
		cur_band = new_reg->numRects;

		r1_band_end = r1;
		while ((r1_band_end != r1_end) && (r1_band_end->y1 == r1->y1))
		{
			r1_band_end++;
		}

		r2_band_end = r2;
		while ((r2_band_end != r2_end) && (r2_band_end->y1 == r2->y1))
		{
			r2_band_end++;
		}

		if (r1->y1 < r2->y1)
		{
			top = MAX(r1->y1,ybot);
			bot = MIN(r1->y2,r2->y1);

			if ((top != bot) && (nonOverlap1Func != NULL))
			{
				nonOverlap1Func(new_reg, r1, r1_band_end, top, bot);
			}

			ytop = r2->y1;
		}
		else if (r2->y1 < r1->y1)
		{
			top = MAX(r2->y1,ybot);
			bot = MIN(r2->y2,r1->y1);

			if ((top != bot) && (nonOverlap2Func != NULL))
			{
				nonOverlap2Func(new_reg, r2, r2_band_end, top, bot);
			}
			ytop = r1->y1;
		}
		else
		{
			ytop = r1->y1;
		}

		if (new_reg->numRects != cur_band)
		{
			prev_band = rdp_region_combine (new_reg, prev_band, cur_band);
		}

		ybot = MIN(r1->y2, r2->y2);
		cur_band = new_reg->numRects;
		if (ybot > ytop)
		{
			overlapFunc(new_reg, r1, r1_band_end, r2, r2_band_end, ytop, ybot);
		}

		if (new_reg->numRects != cur_band)
		{
			prev_band = rdp_region_combine (new_reg, prev_band, cur_band);
		}

		if (r1->y2 == ybot)
		{
			r1 = r1_band_end;
		}
		if (r2->y2 == ybot)
		{
			r2 = r2_band_end;
		}
	} while ((r1 != r1_end) && (r2 != r2_end));


	cur_band = new_reg->numRects;
	if (r1 != r1_end)
	{
		if (nonOverlap1Func != NULL)
		{
			do
			{
				r1_band_end = r1;
				while ((r1_band_end < r1_end) && (r1_band_end->y1 == r1->y1))
				{
					r1_band_end++;
				}
				nonOverlap1Func(new_reg, r1, r1_band_end, MAX(r1->y1,ybot), r1->y2);
				r1 = r1_band_end;
			} while (r1 != r1_end);
		}
	}
	else if ((r2 != r2_end) && (nonOverlap2Func != NULL))
	{
		do
		{
			r2_band_end = r2;
			while ((r2_band_end < r2_end) && (r2_band_end->y1 == r2->y1))
			{
				r2_band_end++;
			}
			nonOverlap2Func(new_reg, r2, r2_band_end, MAX(r2->y1,ybot), r2->y2);
			r2 = r2_band_end;
		} while (r2 != r2_end);
	}

	if (new_reg->numRects != cur_band)
	{
		rdp_region_combine(new_reg, prev_band, cur_band);
	}

	if (new_reg->numRects < (new_reg->size >> 1))
	{
		if (REGION_NOT_EMPTY(new_reg))
		{
			rdpBox* prev_rects = new_reg->rects;
			new_reg->size = new_reg->numRects;
			new_reg->rects = (rdpBox*)xrealloc((char *) new_reg->rects, RDP_BOX_SIZE * new_reg->size);
			if (!new_reg->rects)
			new_reg->rects = prev_rects;
		}
		else
		{
			new_reg->size = 1;
			xfree((char *) new_reg->rects);
			new_reg->rects = (rdpBox*)xmalloc(RDP_BOX_SIZE);
		}
	}
	xfree(old_rects);
	return;
}

static int rdp_region_union_non_overlap(rdpRegion* pReg, rdpBox* r, rdpBox* rEnd, sint16 y1, sint16 y2)
{
	rdpBox*	next_rect;

	next_rect = &pReg->rects[pReg->numRects];

	while (r != rEnd)
	{
		next_rect = rdp_region_check(pReg, next_rect);
		next_rect->x1 = r->x1;
		next_rect->y1 = y1;
		next_rect->x2 = r->x2;
		next_rect->y2 = y2;
		pReg->numRects += 1;
		next_rect++;

		r++;
	}
	return 0;
}

static int rdp_region_union_overlap(rdpRegion* pReg, rdpBox* r1, rdpBox* r1_end, rdpBox* r2, rdpBox* r2_end, sint16 y1, sint16 y2)
{
	rdpBox*	next_rect;

	next_rect = &pReg->rects[pReg->numRects];

	while ((r1 != r1_end) && (r2 != r2_end))
	{
		if (r1->x1 < r2->x1)
		{
			next_rect = rdp_region_merge(pReg, r1, next_rect, y1, y2);
			r1++;
		}
		else
		{
			next_rect = rdp_region_merge(pReg, r2, next_rect, y1, y2);
			r2++;
		}
	}

	if (r1 != r1_end)
	{
		do
		{
			next_rect = rdp_region_merge(pReg, r1, next_rect, y1, y2);
			r1++;
		} while (r1 != r1_end);
	}
	else while (r2 != r2_end)
	{
		next_rect = rdp_region_merge(pReg, r2, next_rect, y1, y2);
		r2++;
	}
	return 0;
}

static int rdp_region_intersect_overlap(rdpRegion* region, rdpBox* r1, rdpBox* r1_end, rdpBox* r2, rdpBox* r2_end, sint16 y1, sint16 y2)
{
	sint16 x1;
	sint16 x2;
	rdpBox*	next_rect;

	next_rect = &region->rects[region->numRects];

	while ((r1 != r1_end) && (r2 != r2_end))
	{
		x1 = MAX(r1->x1,r2->x1);
		x2 = MIN(r1->x2,r2->x2);

		if (x1 < x2)
		{
			next_rect = rdp_region_check(region, next_rect);
			next_rect->x1 = x1;
			next_rect->y1 = y1;
			next_rect->x2 = x2;
			next_rect->y2 = y2;
			region->numRects += 1;
			next_rect++;
		}

		if (r1->x2 < r2->x2)
		{
			r1++;
		}
		else if (r2->x2 < r1->x2)
		{
			r2++;
		}
		else
		{
			r1++;
			r2++;
		}
	}
	return 0;
}

/*****************
 * Exported APIs *
 *****************/

/** Allocator function for a rdp region.
 *  The function will allocate a rdpRegion structure.
 *  Need to be deallocated using rdp_region_free()
 *
 *  @return an allocated structure doesn't contain retangle. Need to be deallocated using rdp_region_free()
 */
rdpRegion* rdp_region_new(void)
{
	rdpRegion* temp;

	temp = xnew(rdpRegion);
	if (!temp)
		return NULL;

	temp->rects = xnew(rdpBox);
	if (!temp->rects)
	{
		xfree(temp);
		return NULL;
	}

	temp->numRects = 0;
	temp->extents.x1 = 0;
	temp->extents.y1 = 0;
	temp->extents.x2 = 0;
	temp->extents.y2 = 0;
	temp->size = 1;
	return temp;
}

/** Deallocator function for the rdpRegion structure.
 *  @param region - pointer to the rdpRegion structure to deallocate.
 *                  On return, this pointer is not valid anymore.
 */
void rdp_region_free(rdpRegion* region)
{
	xfree(region->rects);
	xfree(region);
}

/** Initialize rdp region, and set rectangle and extents number to zero
 *  @param region - Pointer to the rdp_freerdp structure that was initialized by a call to rdp_region_new().
 *  @return true if successful. false otherwise.
 */
boolean rdp_region_init(rdpRegion* region)
{
	if (!region)
		return false;
	if (!region->rects)
	{
		region->rects = xnew(rdpBox);
		region->size = 1;
	}

	if (!region->rects)
		return false;

	region->numRects = 0;
	region->extents.x1 = 0;
	region->extents.y1 = 0;
	region->extents.x2 = 0;
	region->extents.y2 = 0;

	return true;
}

/** Initialize rdp region with one rectangle
 *  @param region - Pointer to the rdp_freerdp structure that was initialized by a call to rdp_region_new().
 *  @param region - Pointer to the rdpRect structure.
 *  @return true if successful. false otherwise.
 */
boolean rdp_region_init_rect(rdpRegion* region, rdpRect* rect)
{
	if (!rect)
		return false;
	if (rect->width == 0 || rect->height == 0)
		return rdp_region_init(region);

	if (!region->rects)
	{
		region->rects = xnew(rdpBox);
		region->size = 1;
	}

	if (!region->rects)
		return false;

	region->numRects = 1;
	region->rects[0].x1 = region->extents.x1 = rect->x;
	region->rects[0].y1 = region->extents.y1 = rect->y;
	region->rects[0].x2 = region->extents.x2 = rect->x + rect->width;
	region->rects[0].y2 = region->extents.y2 = rect->y + rect->height;
	return true;
}

/** Get the number of rectangles in the region
 *  @param region - Pointer to the rdp_freerdp structure that was initialized.
 *  @return the number of rectangles
 */
uint32 rdp_region_length(rdpRegion* region)
{
	if (!region)
		return 0;
	return region->numRects;
}

/** Get the internal rectangles in the region, the number of rectangles can be fetched by rdp_region_length()
 *  @param region - Pointer to the rdp_freerdp structure that was initialized.
 *  @return the list of rectangles
 */
rdpBox* rdp_region_get_rects(rdpRegion* region)
{
	if (!region || !region->rects)
		return NULL;
	return region->rects;
}

/** Get the extents rectangles in the region
 *  @param region - Pointer to the rdp_freerdp structure that was initialized.
 *  @return extents rectangle
 */
rdpBox* rdp_region_get_extents(rdpRegion* region)
{
	if (!region)
		return NULL;
	return &region->extents;
}

/** Computes the union of two regions
 *  @param region1 - The first source region
 *  @param region2 - The second source region
 *  @param new_region - The destination region contains the union of region1 and region2
 *  @return true if successful. false otherwise.
 */
boolean rdp_region_union(rdpRegion* region1, rdpRegion* region2, rdpRegion* new_region)
{
	if ((region1 == region2) || (!region1->numRects))
	{
		if (new_region != region2)
			rdp_region_copy(new_region, region2);
		return true;
	}

	if (!region2->numRects)
	{
		if (new_region != region1)
			rdp_region_copy(new_region, region1);
		return true;
	}

	/* region 1 inside region 2 completely and has only one rectangle */
	if (rdp_region_contains(region1, region2) )
	{
		if (new_region != region1)
			rdp_region_copy(new_region, region1);
		return true;
	}

	/* region 2 inside region 1 completely and has only one rectangle */
	if (rdp_region_contains(region2, region1) )
	{
		if (new_region != region2)
			rdp_region_copy(new_region, region2);
		return true;
	}

	rdp_region_op(new_region, region1, region2, REGION_OP_UNION);

	new_region->extents.x1 = MIN(region1->extents.x1, region2->extents.x1);
	new_region->extents.y1 = MIN(region1->extents.y1, region2->extents.y1);
	new_region->extents.x2 = MAX(region1->extents.x2, region2->extents.x2);
	new_region->extents.y2 = MAX(region1->extents.y2, region2->extents.y2);

	return true;
}

/** Updates the destination region from a union of the rectangle and the source region.
 *  @param rect - The rectangle
 *  @param src - The source region
 *  @param dst - The destination region contains the union of rect and src
 *  @return true if successful. false otherwise.
 */
boolean rdp_region_union_rect(rdpRect* rect, rdpRegion* src, rdpRegion* dst)
{
	rdpRegion region;
	if (!rect->width || !rect->height)
		return false;

	region.rects = &region.extents;
	region.numRects = 1;
	region.extents.x1 = rect->x;
	region.extents.y1 = rect->y;
	region.extents.x2 = rect->x + rect->width;
	region.extents.y2 = rect->y + rect->height;
	region.size = 1;
	return rdp_region_union(&region, src, dst);
}

/** Computes the intersection of two regions
 *  @param region1 - The first source region
 *  @param region2 - The second source region
 *  @param new_region - The destination region contains the intersection of region1 and region2
 *  @return true if successful. false otherwise.
 */
boolean rdp_region_intersect(rdpRegion* region1, rdpRegion* region2, rdpRegion* new_region)
{
	if ( (!(region1->numRects)) || (!(region2->numRects)) ||
		(!RDP_REGION_EXTENTS_OVERLAP(&region1->extents, &region2->extents)) )
	{
		new_region->numRects = 0;
		return false;
	}
	else
		rdp_region_op(new_region, region1, region2, REGION_OP_INTERSECT);

	rdp_region_set_extents(new_region);
	return true;
}
