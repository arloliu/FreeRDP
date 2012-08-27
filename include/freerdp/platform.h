/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Platform Abstraction API
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
#ifndef __PLATFORM_API_H
#define __PLATFORM_API_H
#include "config.h"
#define CUR_PLATFORM FREERDP_PLATFORM

/* private marcos */
#define _PLATFORM_HEADER_STRINGIFY(S) #S
#define _PLFTFORM_HEADER_LOCAL_COMPOSE(PREFIX, NAME) _PLATFORM_HEADER_STRINGIFY(PREFIX##_##NAME.h)
#define _PLFTFORM_HEADER_GLOBAL_COMPOSE(PREFIX, NAME) <PREFIX##_##NAME.h>
#define _PLATFORM_CALLBACK_COMPOSE(PREFIX, NAME, ...) PREFIX##_##NAME ( __VA_ARGS__ )

/*
#if FREERDP_PLATFORM != FREERDP_DEFAULT_PLATFORM
#else
	#define PLATFORM_LOCAL_HEADER_FILE(PREFIX, PLATFORM)  <freerdp/platform.h>
#endif
*/

/* generate local style header file name "PREFIX_PLATFORM.h" */
#define PLATFORM_LOCAL_HEADER_FILE(PREFIX, PLATFORM)  _PLFTFORM_HEADER_LOCAL_COMPOSE(PREFIX, PLATFORM)
/* generate global style header file name <PREFIX_PLATFORM.h> */
#define PLATFORM_GLOBAL_HEADER_FILE(PREFIX, PLATFORM) _PLFTFORM_HEADER_GLOBAL_COMPOSE(PREFIX, PLATFORM)
/* generate platform callback function, PREFIX_PLATFORM(...) */
#define PLATFORM_FUNCTION(PREFIX, PLATFORM, ...) _PLATFORM_CALLBACK_COMPOSE ( PREFIX, PLATFORM, __VA_ARGS__ )

#endif /* __PLATFORM_API_H */
