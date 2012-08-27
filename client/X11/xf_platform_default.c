/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Default platform abstration layer
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

/**
 * This is a template for platform abstraction implementation.
 * Please refer the description in xfreerdp.h
 */
#include <freerdp/codec/nsc.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include "xf_gdi.h"
#include "xf_graphics.h"
#include "xfreerdp.h"

boolean xf_platform_init(xfInfo* xfi)
{
	return true;
}

void xf_platform_uninit(xfInfo* xfi)
{
}

void xf_platform_setup_capabilities(xfInfo* xfi)
{
}

void xf_platform_print_args(void)
{
}

int xf_platform_process_args(rdpSettings* settings, const char* opt, const char* val, void* user_data)
{
	return 0;
}

void xf_platform_register_system_callbacks(xfInfo* xfi)
{
}

void xf_platform_register_gdi_update_callbacks(rdpUpdate* update)
{
}

void xf_platform_register_graphics_bitmap_callbacks(rdpBitmap* bitmap)
{
}
