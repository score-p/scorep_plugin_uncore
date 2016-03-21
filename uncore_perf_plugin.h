/*
 * Copyright (c) 2016, Technische Universität Dresden, Germany
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <stdlib.h>
#include <stdint.h>

#if !defined(BACKEND_SCOREP) && !defined(BACKEND_VTRACE)
#define BACKEND_VTRACE
#endif

#if defined(BACKEND_SCOREP) && defined(BACKEND_VTRACE)
#error "Cannot compile for both VT and Score-P at the same time!\n"
#endif

#ifdef BACKEND_SCOREP
#include <scorep/SCOREP_MetricPlugins.h>
#endif
#ifdef BACKEND_VTRACE
#include <vampirtrace/vt_plugin_cntr.h>
#endif

#define MAX_EVENTS 512

#ifdef BACKEND_SCOREP
    typedef SCOREP_Metric_Plugin_MetricProperties metric_properties_t;
    typedef SCOREP_MetricTimeValuePair timevalue_t;
    typedef SCOREP_Metric_Plugin_Info plugin_info_type;
#endif

#ifdef BACKEND_VTRACE
    typedef vt_plugin_cntr_metric_info metric_properties_t;
    typedef vt_plugin_cntr_timevalue timevalue_t;
    typedef vt_plugin_cntr_info plugin_info_type;
#endif
struct event {
    int32_t node;
    int32_t cpu;
    int32_t scatter_id;
    int32_t enabled;
    void * ID;
    size_t data_count;
    timevalue_t *result_vector;
    char * name;
    int32_t fd;
#ifdef X86_ADAPT
    int32_t item;
#endif
}__attribute__((aligned(64)));

int32_t node_num;
int32_t cpus;

#if 0
int32_t init(void);
void fini(void);
metric_properties_t * get_event_info(char * __event_name);
int32_t add_counter(char * event_name);
uint64_t get_all_values(int32_t id, timevalue_t **result);
#endif
