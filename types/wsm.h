/*
   libSbUNCOREPlugin.so,
   a library to count Uncore events on a SandyBridge E processor for VampirTrace.
   Copyright (C) 2012 TU Dresden, ZIH

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, v2, as
   published by the Free Software Foundation

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef UNCORE_WSM_H
#define UNCORE_WSM_H

#include "../uncore_types.h"

#define WSM_UNCORE_PERF_GLOBAL_CTRL	0x391	

#define for_each_wsm_box(i) \
    for (i=0;i<wsm_uncore_boxes_len;i++) \
        if (pmu_is_present(wsm_uncore_boxes[i].pmu))

struct uncore_box wsm_uncore_boxes[] = {
    {
        MSR,
        8,
        PFM_PMU_INTEL_WSM_UNC,
        MAX_UINT48,
        "NULL",
        0,  
        {0x3c0, 0x3c1, 0x3c2, 0x3c3, 0x3c4, 0x3c5, 0x3c6, 0x3c7}, 
        {0x3b0, 0x3b1, 0x3b2, 0x3b3, 0x3b4, 0x3b5, 0x3b6, 0x3b7}
    }
};

uncore_boxes_len(wsm);

#endif
