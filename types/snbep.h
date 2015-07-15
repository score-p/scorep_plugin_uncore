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

#ifndef UNCORE_SNBEP_H
#define UNCORE_SNBEP_H

#include "../uncore_types.h"


#define for_each_snbep_box(i) \
    for (i=0;i<snbep_uncore_boxes_len;i++) \
        if (pmu_is_present(snbep_uncore_boxes[i].pmu))
        

struct uncore_box snbep_uncore_boxes[] = {
    {
        MSR,
        2,
        PFM_PMU_INTEL_SNBEP_UNC_UBOX, 
        MAX_UINT44,
        "NULL",
        0,
        {0xc10, 0xc11},
        {0xc16, 0xc17}
    },
    {
        MSR,
        4,
        PFM_PMU_INTEL_SNBEP_UNC_PCU,
        MAX_UINT48,
        "NULL",
        0,
        {0xc30,0xc31,0xc32,0xc33},
        {0xc36,0xc37,0xc38,0xc38}
    },
    {
        PCI,
        4,
        PFM_PMU_INTEL_SNBEP_UNC_HA,
        MAX_UINT48,
        "0e.1",
        0,
        {0xd8,0xdc,0xe0,0xe4},
        {0xa0,0xa8,0xb0,0xb8}
    },
    {
        PCI,
        4,
        PFM_PMU_INTEL_SNBEP_UNC_IMC0,
        MAX_UINT48,
        "10.0",
        0,
        {0xd8,0xdc,0xe0,0xe4},
        {0xa0,0xa8,0xb0,0xb8}
    },
    {
        PCI,
        4,
        PFM_PMU_INTEL_SNBEP_UNC_IMC1,
        MAX_UINT48,
        "10.1",
        0,
        {0xd8,0xdc,0xe0,0xe4},
        {0xa0,0xa8,0xb0,0xb8}
    },
    {
        PCI,
        4,
        PFM_PMU_INTEL_SNBEP_UNC_IMC2,
        MAX_UINT48,
        "10.4",
        0,
        {0xd8,0xdc,0xe0,0xe4},
        {0xa0,0xa8,0xb0,0xb8}
    },
    {
        PCI,
        4,
        PFM_PMU_INTEL_SNBEP_UNC_IMC3,
        MAX_UINT48,
        "10.5",
        0,
        {0xd8,0xdc,0xe0,0xe4},
        {0xa0,0xa8,0xb0,0xb8}
    },
    {
        PCI,
        4,
        PFM_PMU_INTEL_SNBEP_UNC_QPI0,
        MAX_UINT48,
        "08.0",
        0,
        {0xd8,0xdc,0xe0,0xe4},
        {0xa0,0xa8,0xb0,0xb8}
    },
    {
        PCI,
        4,
        PFM_PMU_INTEL_SNBEP_UNC_QPI1,
        MAX_UINT48,
        "09.0",
        0,
        {0xd8,0xdc,0xe0,0xe4},
        {0xa0,0xa8,0xb0,0xb8}
    },
    {
        PCI,
        4,
        PFM_PMU_INTEL_SNBEP_UNC_R2PCIE,
        MAX_UINT44,
        "13.1",
        0,
        {0xd8,0xdc,0xe0,0xe4},
        {0xa0,0xa8,0xb0,0xb8}
    },
    {
        PCI,
        3,
        PFM_PMU_INTEL_SNBEP_UNC_R3QPI0,
        MAX_UINT44,
        "13.5",
        0,
        {0xd8,0xdc,0xe0},
        {0xa0,0xa8,0xb0}
    },
    {
        PCI,
        3,
        PFM_PMU_INTEL_SNBEP_UNC_R3QPI1,
        MAX_UINT44,
        "13.6",
        0,
        {0xd8,0xdc,0xe0},
        {0xa0,0xa8,0xb0}
    }
};

uncore_boxes_len(snbep);


#endif
