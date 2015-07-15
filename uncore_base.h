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

//better dependency
#include <perfmon/pfmlib.h>
#include "uncore_types.h"

uint64_t read_uncore_box(int);
int uncore_init(void);
void uncore_fini(void);
int uncore_setup(pfm_pmu_encode_arg_t*, pfm_event_info_t*, char *, char**);
int arch_of(pfm_pmu_t);
int pmu_to_box(pfm_pmu_t, struct uncore_box**);
int pmu_is_present(pfm_pmu_t);
const char * pmu_to_str(pfm_pmu_t);
