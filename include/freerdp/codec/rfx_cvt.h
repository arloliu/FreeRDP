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
#ifndef __FREERDP_RFX_PLAT_CVT_H
#define __FREERDP_RFX_PLAT_CVT_H

struct _RFX_CONTEXT_CVT_PRIV
{
	int fd;
	uint8 rfx_cin_status;
	uint8 rfx_op_mode;
	uint8 rfx_mon_layout;
};
typedef struct _RFX_CONTEXT_CVT_PRIV RFX_CONTEXT_CVT_PRIV;

#endif /* __FREERDP_RFX_PLAT_CVT_H */

