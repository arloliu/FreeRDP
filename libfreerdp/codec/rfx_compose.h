/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Composer
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

struct _RFX_COMPOSE_CONTEXT_PRIV
{
	sint16 y_r_mem[4096 + 8]; /* 4096 = 64x64 (+ 8x2 = 16 for mem align) */
	sint16 cb_g_mem[4096 + 8]; /* 4096 = 64x64 (+ 8x2 = 16 for mem align) */
	sint16 cr_b_mem[4096 + 8]; /* 4096 = 64x64 (+ 8x2 = 16 for mem align) */
 
 	sint16* y_r_buffer;
	sint16* cb_g_buffer;
	sint16* cr_b_buffer;
 
	sint16 dwt_mem[32 * 32 * 2 * 2 + 8]; /* maximum sub-band width is 32 */

	sint16* dwt_buffer;

	void (*encode_rgb_to_ycbcr)(sint16* y_r_buf, sint16* cb_g_buf, sint16* cr_b_buf);
	void (*quantization_encode)(sint16* buffer, const uint32* quantization_values);
	void (*dwt_2d_encode)(sint16* buffer, sint16* dwt_buffer);

	PROFILER_DEFINE(prof_rfx_encode_rgb);
	PROFILER_DEFINE(prof_rfx_encode_component);
	PROFILER_DEFINE(prof_rfx_rlgr_encode);
	PROFILER_DEFINE(prof_rfx_differential_encode);
	PROFILER_DEFINE(prof_rfx_quantization_encode);
	PROFILER_DEFINE(prof_rfx_dwt_2d_encode);
	PROFILER_DEFINE(prof_rfx_encode_rgb_to_ycbcr);
	PROFILER_DEFINE(prof_rfx_encode_format_rgb);
};
