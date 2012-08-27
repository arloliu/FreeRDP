/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Cloudvue CVT8311 ASIC platform abstration layer
 *
 * Copyright 2012 Cloudvue Technologies Corp.
 * Copyright 2012 Atrust Computer Corp.
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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ioctl.h>

#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/constants.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/args.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/platform/cvt/cvt.h>

#include "xf_gdi.h"
#include "xf_graphics.h"
#include "xfreerdp.h"
#include "xf_platform_cvt.h"


static boolean xf_cvt_detect_monitors(xfInfo* xfi, rdpSettings* settings)
{
	int i;
	VIRTUAL_SCREEN* vscreen;
	rdpCvtSettings *cvt_settings;
	uint8 monitor_layout;
	
	cvt_settings = (rdpCvtSettings*) settings->priv;
	monitor_layout = cvt_settings->monitor_layout;

	vscreen = &xfi->vscreen;

	if (xf_GetWorkArea(xfi) != true)
	{
		xfi->workArea.x = 0;
		xfi->workArea.y = 0;
		xfi->workArea.width = WidthOfScreen(xfi->screen);
		xfi->workArea.height = HeightOfScreen(xfi->screen);
	}

	/* Get full screen width & height */
	settings->width = WidthOfScreen(xfi->screen);
	settings->height = HeightOfScreen(xfi->screen);	

	for (i = 0; i < settings->num_monitors; i++)
	{
		if ((monitor_layout == MON_LAYOUT_T0_B1) || (monitor_layout == MON_LAYOUT_T1_B0))
		{
			settings->monitors[i].x = 0;
			settings->monitors[i].y = i * settings->height;
		}
		else
		{
			settings->monitors[i].x = i * settings->width;
			settings->monitors[i].y = 0;
		}
		settings->monitors[i].width = settings->width;
		settings->monitors[i].height = settings->height;
		settings->monitors[i].is_primary = (i == 0);
	}
	
	if ((monitor_layout == MON_LAYOUT_T0_B1) || (monitor_layout == MON_LAYOUT_T1_B0))
		settings->height = settings->height * settings->num_monitors;
	else
		settings->width = settings->width * settings->num_monitors;

	xfi->vscreen.area.left = 0;
	xfi->vscreen.area.top = 0;
	xfi->vscreen.area.right = settings->width;
	xfi->vscreen.area.bottom = settings->height;
	
	return true;
}

/**
 * This function is called by xf_post_connect
 */
static void xf_cvt_codec_context_create(xfInfo* xfi)
{
	RFX_CONTEXT* rfx_context = NULL;
	NSC_CONTEXT* nsc_context = NULL;
	rdpSettings* settings;
	rdpCvtSettings* cvt_settings;

	settings = xfi->instance->settings;
	cvt_settings = (rdpCvtSettings*) settings->priv;

	/*
	 * Check if server support frame acknowledge, and set server type,
	 * It is a trick for Windows 7/WMS 2011/Windows 2008 R2
	 */
	if (settings->received_caps[CAPSET_TYPE_FRAME_ACKNOWLEDGE])
		cvt_settings->server_type = SERVER_TYPE_RDVH;
	else
		cvt_settings->server_type = SERVER_TYPE_RDSH;

	if (settings->ns_codec)
	{
		nsc_context = (void*) nsc_context_new();
	}

	if (settings->rfx_codec)
	{
		if (settings->server_mode)
		{
			printf("XRDP Server\n");
			if (cvt_settings->opmode == 0 || cvt_settings->opmode == 1)
			{
				cvt_settings->opmode = RDSH;
			}
			else
				cvt_settings->opmode = RDSH_OPT;
		}
		else if (cvt_settings->server_type == SERVER_TYPE_RDSH)
		{
			switch (cvt_settings->opmode)
			{
				case 0: /* auto select mode */
				case 2: /* enable optimization */
					cvt_settings->opmode = RDSH_OPT;
					break;
				case 1: /* disable optimization */
					cvt_settings->opmode = RDSH;
					break;
				default:
					cvt_settings->opmode = RDSH;
					break;
				
			};
		}
		else if (cvt_settings->server_type == SERVER_TYPE_RDVH)
		{
			switch (cvt_settings->opmode)
			{
				case 0: /* auto select mode */
				case 1: /* disable optimization */
					cvt_settings->opmode = RDVH;
					break;
				case 2: /* enable optimization */
					cvt_settings->opmode = RDVH_OPT;
					break;
				default:
					cvt_settings->opmode = RDVH;
					break;
			};
		}

		if (cvt_settings->opmode == RDSH)
			printf("RemoteFX RDSH mode\n");
		else if (cvt_settings->opmode == RDSH_OPT)
			printf("RemoteFX RDSH Optimization mode\n");					
		else if (cvt_settings->opmode == RDVH)
			printf("RemoteFX RDVH mode\n");	
		else if (cvt_settings->opmode == RDVH_OPT)
			printf("RemoteFX RDVH Optimization mode\n");					

		rfx_context = rfx_context_new(settings);
	}

	xfi->rfx_context = (void*) rfx_context;
	xfi->nsc_context = (void*) nsc_context;
}

static void xf_cvt_codec_context_free(xfInfo* xfi)
{
	if (xfi->rfx_context) 
	{
		rfx_context_free(xfi->rfx_context);
		xfi->rfx_context = NULL;
	}

	if (xfi->nsc_context)
	{
		nsc_context_free(xfi->nsc_context);
		xfi->nsc_context = NULL;
	}
}

static void xf_cvt_gdi_surface_frame_marker(rdpContext* context, SURFACE_FRAME_MARKER* surface_frame_marker)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;
	RFX_CONTEXT* rfx_context;
	direct_blt_st dblt;
	cvU32 frame_action;

	if (context->instance->settings->rfx_codec)
	{
		rfx_context = (RFX_CONTEXT*) xfi->rfx_context;
		RFX_CONTEXT_CVT_PRIV* priv = (RFX_CONTEXT_CVT_PRIV*) rfx_context->priv;
		if (surface_frame_marker->frameAction == SURFACECMD_FRAMEACTION_END)
		{
			frame_action = CVT_FRAME_END;
			ioctl(priv->fd, DEV_IOCTL_FRAME_ACTION, &frame_action);

			if (priv->rfx_cin_status)
			{
				dblt.flag = DIRECTBLT_GOGOGO;
				ioctl(priv->fd, DEV_IOCTL_DIRECT_BLT, &dblt);
				priv->rfx_cin_status = 0;
			} 
		}
		return;
	}

	switch (surface_frame_marker->frameAction)
	{
		case SURFACECMD_FRAMEACTION_BEGIN:
			xfi->frame_begin = true;
			xfi->frame_x1 = 0;
			xfi->frame_y1 = 0;
			xfi->frame_x2 = 0;
			xfi->frame_y2 = 0;
			break;

		case SURFACECMD_FRAMEACTION_END:
			xfi->frame_begin = false;
			if (xfi->frame_x2 > xfi->frame_x1 && xfi->frame_y2 > xfi->frame_y1)
			{
				XSetFunction(xfi->display, xfi->gc, GXcopy);
				XSetFillStyle(xfi->display, xfi->gc, FillSolid);

				XCopyArea(xfi->display, xfi->primary, xfi->drawable, xfi->gc,
					xfi->frame_x1, xfi->frame_y1,
					xfi->frame_x2 - xfi->frame_x1, xfi->frame_y2 - xfi->frame_y1,
					xfi->frame_x1, xfi->frame_y1);
				gdi_InvalidateRegion(xfi->hdc, xfi->frame_x1, xfi->frame_y1,
					xfi->frame_x2 - xfi->frame_x1, xfi->frame_y2 - xfi->frame_y1);
			}
			break;
	}
}

static inline void xf_cvt_set_direct_blit(direct_blt_st* dblt, SURFACE_BITS_COMMAND* command, RFX_CONTEXT* rfx_context, uint8 mon_layout)
{
	switch (mon_layout)
	{	
		case MON_LAYOUT_S0:
		case MON_LAYOUT_CLONE:
			dblt->display_num = 0;
			dblt->x = command->destLeft;
			dblt->y = command->destTop;
			break;
		case MON_LAYOUT_S1:
			dblt->display_num = 1;
			dblt->x = command->destLeft;
			dblt->y = command->destTop;
			break;
		case MON_LAYOUT_L0_R1:
			if (command->destLeft < rfx_context->width)
			{
				dblt->display_num = 0;
				dblt->x = command->destLeft;
				dblt->y = command->destTop;
			}
			else
			{
				dblt->display_num = 1;
				dblt->x = command->destLeft - rfx_context->width;
				dblt->y = command->destTop;
			}
			break;
		case MON_LAYOUT_L1_R0:
			if (command->destLeft < rfx_context->width)
			{
				dblt->display_num = 1;
				dblt->x = command->destLeft;
				dblt->y = command->destTop;
			}
			else
			{
				dblt->display_num = 0;
				dblt->x = command->destLeft - rfx_context->width;
				dblt->y = command->destTop;
			}
			break;
		case MON_LAYOUT_T0_B1:
			if (command->destTop < rfx_context->height)
			{
				dblt->display_num = 0;
				dblt->x = command->destLeft;
				dblt->y = command->destTop;
			}
			else
			{
				dblt->display_num = 1;
				dblt->x = command->destLeft;
				dblt->y = command->destTop - rfx_context->height;
			}
			break;
		case MON_LAYOUT_T1_B0:
			if (command->destTop < rfx_context->height)
			{
				dblt->display_num = 1;
				dblt->x = command->destLeft;
				dblt->y = command->destTop;
			}
			else
			{
				dblt->display_num = 0;
				dblt->x = command->destLeft;
				dblt->y = command->destTop - rfx_context->height;
			}
			break;
		default:
			dblt->display_num = 0;
			dblt->x = command->destLeft;
			dblt->y = command->destTop;
			break;
	}
}


static void xf_cvt_gdi_surface_rfx_process(rdpContext* context, SURFACE_BITS_COMMAND* command, void* pContext)
{
	RECTANGLE_16 dest_rect;
	RFX_MESSAGE* message = NULL;
	RFX_CONTEXT* rfx_context = (RFX_CONTEXT*) pContext;
	RFX_CONTEXT_CVT_PRIV* priv = (RFX_CONTEXT_CVT_PRIV* )rfx_context->priv;
	direct_blt_st dblt;

	dest_rect.left = command->destLeft;
	dest_rect.top = command->destTop;
	dest_rect.right = command->destRight;
	dest_rect.bottom = command->destBottom;

	if (priv->rfx_cin_status)
	{
		dblt.flag = DIRECTBLT_GOGOGO;
		ioctl(priv->fd, DEV_IOCTL_DIRECT_BLT, &dblt);
		priv->rfx_cin_status = 0;
	} 
	message = rfx_process_message(rfx_context, command->bitmapData, command->bitmapDataLength, &dest_rect);
	rfx_message_free(rfx_context, message);
}

static void xf_cvt_gdi_surface_nsc_process(RFX_CONTEXT* rfx_context, SURFACE_BITS_COMMAND* command, NSC_CONTEXT* nsc_context)
{
	RFX_CONTEXT_CVT_PRIV* priv = (RFX_CONTEXT_CVT_PRIV* )rfx_context->priv;
	direct_blt_st dblt;


	nsc_process_message(nsc_context, command->bpp, command->width, command->height,
		command->bitmapData, command->bitmapDataLength);

	DEBUG_X11("CODEC_ID_NSC: %d %d %d %d 0x%08x %d", 
		command->destLeft, command->destTop,
		command->width, command->height,
		(uint32)nsc_context->bmpdata, command->bitmapDataLength);

	xf_cvt_set_direct_blit(&dblt, command, rfx_context, priv->rfx_mon_layout);

	dblt.p_src_buf = nsc_context->bmpdata;
	dblt.width     = command->width;
	dblt.height    = command->height;

	if (priv->rfx_cin_status)
		dblt.flag = DIRECTBLT_APPEND;
	else
	{
		dblt.flag = DIRECTBLT_CREATE;
		priv->rfx_cin_status = 1;
	}
	ioctl(priv->fd, DEV_IOCTL_DIRECT_BLT, &dblt);
}

static void xf_cvt_gdi_surface_bitmap_process(RFX_CONTEXT* context, SURFACE_BITS_COMMAND* command)
{
	RFX_CONTEXT_CVT_PRIV* priv = (RFX_CONTEXT_CVT_PRIV* )context->priv;
	direct_blt_st dblt;

	DEBUG_X11("CODEC_ID_NONE: %d %d %d %d 0x%08x %d", 
		command->destLeft, command->destTop,
		command->width, command->height,
		(uint32)command->bitmapData, command->bitmapDataLength);

	xf_cvt_set_direct_blit(&dblt, command, context, priv->rfx_mon_layout);

	dblt.p_src_buf = command->bitmapData;
	dblt.width     = command->width;
	dblt.height    = command->height;

	if (priv->rfx_cin_status)
		dblt.flag = DIRECTBLT_APPEND;
	else
	{
		dblt.flag = DIRECTBLT_CREATE;
		priv->rfx_cin_status = 1;
	}
	ioctl(priv->fd, DEV_IOCTL_DIRECT_BLT, &dblt);	
}

void xf_cvt_gdi_surface_bits(rdpContext* context, SURFACE_BITS_COMMAND* surface_bits_command)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;
	rdpUpdate* update = context->instance->update;
	RFX_CONTEXT* rfx_context = (RFX_CONTEXT*) xfi->rfx_context;
	NSC_CONTEXT* nsc_context = (NSC_CONTEXT*) xfi->nsc_context;

	if (surface_bits_command->codecID == CODEC_ID_REMOTEFX)
	{
		IFCALL(update->SurfaceRfxProcess, context, surface_bits_command, rfx_context);
	}
	else if (surface_bits_command->codecID == CODEC_ID_NSCODEC)
	{
		if (context->instance->settings->rfx_codec)
			xf_cvt_gdi_surface_nsc_process(rfx_context, surface_bits_command, nsc_context);
		else
			IFCALL(update->SurfaceNscProcess, context, surface_bits_command, nsc_context);
	}
	else if (surface_bits_command->codecID == CODEC_ID_NONE)
	{
		if (context->instance->settings->rfx_codec)
			xf_cvt_gdi_surface_bitmap_process(rfx_context, surface_bits_command);
		else
			IFCALL(update->SurfaceBitmapProcess, context, surface_bits_command, NULL);
	}
	else
	{
		printf("Unsupported codecID %d\n", surface_bits_command->codecID);
	}
}

/****************************************************************/
boolean xf_platform_init(xfInfo* xfi)
{
	rdpCvtSettings* cvt_settings;
	rdpSettings* settings;

	xfi->priv = xnew(xfiCvtPriv);
	xfi->context->settings->priv = xnew(rdpCvtSettings);

	settings = xfi->context->settings;
	cvt_settings = xfi->context->settings->priv;

	settings->num_monitors = 1;
	cvt_settings->server_type = SERVER_TYPE_RDSH;
	cvt_settings->opmode = 0;
	cvt_settings->monitor_layout = MON_LAYOUT_S0;
	cvt_settings->hw_crypto_eng = false;
	cvt_settings->fb_num = 0;
	
	return true;
}

void xf_platform_uninit(xfInfo* xfi)
{
	xfiCvtPriv* priv = (xfiCvtPriv*)xfi->priv;

	if (!priv)
		return;

	if (xfi->context && xfi->context->settings)
		xfree(xfi->context->settings->priv);
	xfree(priv);
}

void xf_platform_setup_capabilities(xfInfo* xfi)
{
	rdpSettings* settings;
	settings = xfi->context->settings;
	
	if (settings->rfx_codec)
	{
		/* CVT8311 doesn't support mixed mode and only support RLGR1 entropy */
		settings->rfx_codec_mode = RFX_CODEC_MODE_VIDEO;
		settings->rfx_mix_mode = true;
		settings->rfx_codec_entropy = RFX_CODEC_ENTROPY_RLGR1;

		settings->fastpath_output = true;
		settings->color_depth = 32;
		settings->frame_acknowledge = 4;
		settings->large_pointer = true;

		/* Prefer to use Windows Aero and smooth font, it's has better performance on CVT8311 */
		settings->performance_flags |= PERF_ENABLE_FONT_SMOOTHING;
		settings->performance_flags |= PERF_ENABLE_DESKTOP_COMPOSITION;

		/* Prefer not to use NSCodec*/
		settings->ns_codec = false;
		
		/* Don't use software GDI at all */
		xfi->sw_gdi = false;

		/* CVT8311 only supports full screen RemoteFX */
		xfi->fullscreen_toggle = false;
		settings->fullscreen = true;
	}
}

void xf_platform_print_args(void)
{
	printf("CVT8311 Options\n"
		"  --rfx-cvt-opt : RemoteFX optimization mode (enable=2,disable=1, auto=0), default is 0\n"
		"  --nm : Number of monitors, default is 1\n"
		"  --lm : Monitor layout mode (S0=0, S1=1, L0_R1=2, L1_R0=3, T0_B1=4, T1_B0=5), default is 0\n"
		"  --hw-crypto-eng : enable hardware encryption/decription engine \n"
		"\n"
	);
}

int xf_platform_process_args(rdpSettings* settings, const char* opt, const char* val, void* user_data)
{
	rdpCvtSettings* cvt_settings;
	int argc;

	cvt_settings = (rdpCvtSettings*)settings->priv;
	argc = 0;
	if (strcmp("--rfx-cvt-opt", opt) == 0)
	{
		argc = 2;
		if (!val)
		{
			printf("missing CVT OP mode parameters\n");
			return FREERDP_ARGS_PARSE_FAILURE;
		}
		cvt_settings->opmode = atoi(val);
		if (cvt_settings->opmode > 2 || cvt_settings->opmode < 0)
		{
			printf("optimization mode has to be 2(enable), 1(disable) or 0(auto)\n");
			return FREERDP_ARGS_PARSE_FAILURE;
		}
	}
	else if (strcmp("-nm", opt) == 0)
	{
		argc = 2;
		if (!val)
		{
			printf("missing display number\n");
			return FREERDP_ARGS_PARSE_FAILURE;
		}
		settings->num_monitors = atoi(val);
		if ((settings->num_monitors != 2) && (settings->num_monitors != 1))
		{
			printf("not supported display number %d\n",settings->num_monitors);
			return FREERDP_ARGS_PARSE_FAILURE;
		}
	}
	else if (strcmp("-lm", opt) == 0)
	{
		argc = 2;
		if (!val)
		{
			printf("missing monitor layout paramters\n");
			return FREERDP_ARGS_PARSE_FAILURE;
		}
		cvt_settings->monitor_layout = atoi(val);
		if ((cvt_settings->monitor_layout > 6))
		{
			printf("not supported monitor layout mode %d\n",cvt_settings->monitor_layout);
			return FREERDP_ARGS_PARSE_FAILURE;
		}
	}
	else if (strcmp("-fb", opt) == 0)
	{
		argc = 2;
		if (!val)
		{
			printf("missing frame buffer parameters\n");
			return FREERDP_ARGS_PARSE_FAILURE;
		}
		cvt_settings->fb_num = atoi(val);

	}
	else if (strcmp("--hw-crypto-eng", opt) == 0)
	{
		argc = 1;
		cvt_settings->hw_crypto_eng = true;
	}
	return argc;
}

void xf_platform_register_system_callbacks(xfInfo* xfi)
{
	xfi->MonitorDetect = xf_cvt_detect_monitors;
	xfi->CodecContextCreate = xf_cvt_codec_context_create;
	xfi->CodecContextFree = xf_cvt_codec_context_free;
}

void xf_platform_register_gdi_update_callbacks(rdpUpdate* update)
{
	update->SurfaceFrameMarker = xf_cvt_gdi_surface_frame_marker;
	update->SurfaceBits = xf_cvt_gdi_surface_bits;
	update->SurfaceRfxProcess = xf_cvt_gdi_surface_rfx_process;
}

void xf_platform_register_graphics_bitmap_callbacks(rdpBitmap* bitmap)
{
}
