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

#ifndef UNCORE_TYPES_H
#define UNCORE_TYPES_H

#include <stdlib.h>
#include <vampirtrace/vt_plugin_cntr.h>
#include <perfmon/pfmlib.h>
#include <msr.h>

#define MAX_UINT44 0xfffffffffff
#define MAX_UINT48 0xffffffffffff

enum box_type {MSR, PCI};

#define uncore_boxes_len(arch) \
    int arch##_uncore_boxes_len = sizeof(arch##_uncore_boxes)/sizeof(arch##_uncore_boxes[0])

/*
#define uncore_boxes(arch) \
        extern struct uncore_box arch##_uncore_boxes[]; \
        extern int arch##_uncore_boxes_len \
        */

enum arch {ARCH_UNSUPPORTED, INTEL_WESTMERE, INTEL_SANDYBRIDGE_EP, AMD_FAM15h};

struct uncore_box {
    enum box_type type;
    int max_cntr;
    pfm_pmu_t pmu;
    uint64_t max;
    char counter[32];
    int curr;
    int cfg[10];
    int reg[10];
};

struct event {
    int32_t init;
    int32_t node;
    int32_t enabled;
    void * ID;
    int32_t data_count;
    vt_plugin_cntr_timevalue *result_vector; 
    pthread_mutex_t mutex;
    char * name;
    union {
        uint32_t rdpmc_counter;
        struct {
           int fd;
           uint32_t reg;
        };
        struct msr_handle handle;
    };
    struct uncore_box * box;
    uint64_t last;
};

#endif
