/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Cloudvue CVT8311 ASIC platform abstration layer
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
#ifndef __XF_PLATFORM_CVT_H
#define __XF_PLATFORM_CVT_H
#include "xfreerdp.h"
#include "xf_gdi.h"
#include "xf_graphics.h"

/* server type */
#define SERVER_TYPE_RDSH			0x00
#define SERVER_TYPE_RDVH			0x01
#define SERVER_TYPE_XRDP			0x02


struct xfi_cvt_priv
{
	uint8 server_type;
	int opmode;
	uint8 monitor_layout;
	boolean hw_crypto_eng;
	uint8 fb_num; /* /dev/fbn  (n=0,1,...) */	
};
typedef struct xfi_cvt_priv xfiCvtPriv;

#endif /* __XF_PLATFORM_CVT_H */
