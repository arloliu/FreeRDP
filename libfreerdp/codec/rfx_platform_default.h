/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec default platform
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

#ifndef __RFX_PLATFORM_DEFAULT_H
#define __RFX_PLATFORM_DEFAULT_H

#include "rfx_pool.h"

struct _RFX_CONTEXT_DEFAULT_PRIV
{
	/* pre-allocated buffers */
	RFX_POOL* pool; /* memory pool */

	sint16 y_r_mem[4096 + 8]; /* 4096 = 64x64 (+ 8x2 = 16 for mem align) */
	sint16 cb_g_mem[4096 + 8]; /* 4096 = 64x64 (+ 8x2 = 16 for mem align) */
	sint16 cr_b_mem[4096 + 8]; /* 4096 = 64x64 (+ 8x2 = 16 for mem align) */
 
 	sint16* y_r_buffer;
	sint16* cb_g_buffer;
	sint16* cr_b_buffer;
 
	sint16 dwt_mem[32 * 32 * 2 * 2 + 8]; /* maximum sub-band width is 32 */

	sint16* dwt_buffer;

	/* routines */
	void (*decode_ycbcr_to_rgb)(sint16* y_r_buf, sint16* cb_g_buf, sint16* cr_b_buf);
	void (*quantization_decode)(sint16* buffer, const uint32* quantization_values);
	void (*dwt_2d_decode)(sint16* buffer, sint16* dwt_buffer);


	/* profilers */
	PROFILER_DEFINE(prof_rfx_decode_rgb);
	PROFILER_DEFINE(prof_rfx_decode_component);
	PROFILER_DEFINE(prof_rfx_rlgr_decode);
	PROFILER_DEFINE(prof_rfx_differential_decode);
	PROFILER_DEFINE(prof_rfx_quantization_decode);
	PROFILER_DEFINE(prof_rfx_dwt_2d_decode);
	PROFILER_DEFINE(prof_rfx_decode_ycbcr_to_rgb);
	PROFILER_DEFINE(prof_rfx_decode_format_rgb);
};
typedef struct _RFX_CONTEXT_DEFAULT_PRIV RFX_CONTEXT_DEFAULT_PRIV;

#endif /* __RFX_PLATFORM_DEFAULT_H */
