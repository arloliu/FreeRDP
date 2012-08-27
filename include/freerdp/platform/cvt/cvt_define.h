/*
 * Copyright (c) 2011 by Cloudvue Technologies, Inc.  ALL RIGHTS RESERVED.
 * These coded instructions, statements, and computer programs are the copyrighted
 * works and confidential proprietary information of Cloudvue Technologies, Inc.
 * They may not be modified, copied, reproduced, distributed, or disclosed
 * to third parties in any manner, medium, or form, in whole or in part,
 * without the prior written consent of Cloudvue Technologies, Inc.
 */

#ifndef __CVT_DEFINE_H__
#define	__CVT_DEFINE_H__

// -------- ASIC or FPGA -------------
// comment define CONFIG_ASIC for FPGA
//------------------------------------
#define	CONFIG_ASIC
#if !defined (CONFIG_ASIC) && (defined (CONFIG_MACH_MVKW6180_CV2) || defined (CONFIG_MACH_AT91SAM9G45_CV2))
#define	CONFIG_ASIC
#endif

typedef unsigned char           cvU8;
typedef unsigned short          cvU16;
typedef unsigned int            cvU32;
typedef signed char             cvS8;
typedef signed short            cvS16;
typedef signed int              cvS32;
typedef unsigned long long      cvU64;

#if !defined(FAILURE) || (FAILURE!=(-1))
#define FAILURE   (-1)    
#endif

#if !defined(SUCCESS) || (SUCCESS!=0)
#define SUCCESS   0
#endif

#if !defined(OK) || (OK!=0)
#define OK        0 
#endif

#if !defined(ERROR) || (ERROR!=(1))
#define ERROR    (1)
#endif

#if !defined(FAIL) || (FAIL!=(-1))
#define FAIL     (-1)  	
#endif

#if !defined(BUSY) || (BUSY!=(1))
#define BUSY     (1)  	
#endif

#if !defined(AVAILABLE) || (AVAILABLE!=(0))
#define AVAILABLE (0)  	
#endif

#if !defined(YES) || (YES!=(1))
#define YES (1)  	
#endif

#if !defined(NO) || (NO!=(0))
#define NO (0)  	
#endif

#define CVT_FB_NAME  "cvtfb"
#define	CVT_DEV_NAME "ct8311"


#define BIT0                    0x00000001
#define BIT1                    0x00000002
#define BIT2                    0x00000004
#define BIT3                    0x00000008
#define BIT4                    0x00000010
#define BIT5                    0x00000020
#define BIT6                    0x00000040
#define BIT7                    0x00000080
#define BIT8                    0x00000100
#define BIT9                    0x00000200
#define BIT10                   0x00000400
#define BIT11                   0x00000800
#define BIT12                   0x00001000
#define BIT13                   0x00002000
#define BIT14                   0x00004000
#define BIT15                   0x00008000
#define BIT16                   0x00010000
#define BIT17                   0x00020000
#define BIT18                   0x00040000
#define BIT19                   0x00080000
#define BIT20                   0x00100000
#define BIT21                   0x00200000
#define BIT22                   0x00400000
#define BIT23                   0x00800000
#define BIT24                   0x01000000
#define BIT25                   0x02000000
#define BIT26                   0x04000000
#define BIT27                   0x08000000
#define BIT28                   0x10000000
#define BIT29                   0x20000000
#define BIT30                   0x40000000
#define BIT31                   0x80000000


#define BUS_SIZE_16             0
#define BUS_SIZE_32             1
#define BUS_SIZE_64             2

// hardware related
#define MAX_BARS                5
#define MAX_ENG_2D              4
#define MAX_SYNC                4

#endif	// __CVT_DEFINE_H__
