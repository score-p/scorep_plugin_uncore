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

#ifndef UNCORE_FAM15H_H
#define UNCORE_FAM15H_H

#include "../uncore_types.h"

#define for_each_fam15h_box(i) \
    for (i=0;i<fam15h_uncore_boxes_len;i++) \
        if (pmu_is_present(fam15h_uncore_boxes[i].pmu))

struct uncore_box fam15h_uncore_boxes[] = {
    {
        MSR,
        4,
        PFM_PMU_AMD64_FAM15H_INTERLAGOS,
        MAX_UINT48,
        "NULL",
        0,  
        {0xc0010240, 0xc0010242, 0xc0010244, 0xc0010246}, 
        {0xc0010241, 0xc0010243, 0xc0010245, 0xc0010247}
    }
};

uncore_boxes_len(fam15h);

#endif
