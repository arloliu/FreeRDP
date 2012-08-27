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
#ifndef __CVT_PLATFORM_H
#define __CVT_PLATFORM_H

#include "cvt_ioctl.h"
#include <freerdp/types.h>

struct _rdp_cvt_settings
{
	uint8 server_type;
	sint32 opmode;
	uint8 monitor_layout;
	boolean hw_crypto_eng;
	uint8 fb_num;	/* /dev/fbn  (n=0,1,...) */	
};
typedef struct _rdp_cvt_settings rdpCvtSettings;
#endif /* __CVT_PLATFORM_H */
