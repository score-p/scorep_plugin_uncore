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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <fcntl.h>
#include <unistd.h>

#include <vampirtrace/vt_plugin_cntr.h>
#include <perfmon/pfmlib.h>

#include <x86_energy.h>

#include "uncore_base.h"
#include "uncore_plugin.h"


struct event event_list[512];
int event_list_size=0;

static pthread_t thread;
static int counter_enabled;
static int is_thread_created = 0;
static int is_sorted = 0;
int node_num;
int cpus;

static uint64_t (*wtime)(void) = NULL;

static int max_data_count = 100;
static int interval_us = 100000;

void set_pform_wtime_function(uint64_t(*pform_wtime)(void)) 
{
    wtime = pform_wtime;
}

int check_cpuid(void)
{
    int i, ret;
    pfm_pmu_info_t pinfo;

	memset(&pinfo, 0, sizeof(pinfo));

    for(i=0; i < PFM_PMU_MAX; i++) {
        ret = pfm_get_pmu_info(i, &pinfo);
        if (ret != PFM_SUCCESS)
            continue;
        if (!pinfo.is_present)
            continue;
        if (arch_of(pinfo.pmu) != ARCH_UNSUPPORTED)
            return 1;
    }

    return 0;
}

static pthread_mutex_t add_counter_mutex;

int32_t init(void)
{
    char * env_string;
    int ret;

    /* get number of packages */
    node_num = x86_energy_get_nr_packages();

     env_string = getenv("VT_UNCORE_INTERVAL_US");
     if (env_string == NULL)
         interval_us = 100000;
     else {
         interval_us = atoi(env_string);
         if (interval_us == 0) {
             fprintf(stderr,
                     "Could not parse VT_UNCORE_INTERVAL_US, using 100 ms\n");
             interval_us = 100000;
         }
     }

     env_string = getenv("VT_UNCORE_BUF_SIZE");
     if (env_string == NULL)
         max_data_count = 100;
     else {
         max_data_count = atoi(env_string);
         if (interval_us == 0) {
             fprintf(stderr,
                     "Could not parse VT_UNCORE_BUF_SIZE, using 100 elements\n");
            max_data_count = 100;
         }
     }

    cpus = sysconf(_SC_NPROCESSORS_CONF);

	ret = pfm_initialize();
	if (ret != PFM_SUCCESS) {
		fprintf(stderr, "cannot initialize library: %s\n", pfm_strerror(ret));
        return -1;
    }

    if (!check_cpuid()) {
        fprintf(stderr,"VT_UNCORE_PLUGIN: Unsupported CPU\n");
        exit(-1);
    } 

    if (uncore_init())
        return -1;
        
    /* check if pthread mutex can be created */
    return pthread_mutex_init( &add_counter_mutex, NULL );
}

vt_plugin_cntr_metric_info * get_event_info(char * __event_name)
{
    int ret;
    unsigned int i;
    char * event_name = strdup(__event_name);
    char * fstr = NULL;
	pfm_pmu_encode_arg_t enc_evt;
	pfm_event_info_t info;
    
    for (i=0; i<strlen(event_name);i++) 
        if (event_name[i] == '#')
            event_name[i] = ':';

    memset(&enc_evt, 0, sizeof(enc_evt));
	memset(&info, 0, sizeof(info));
    enc_evt.fstr = &fstr;

    char **event_arr = alloca(32 * sizeof(char*));
    //for (i=0;i<32;i++)
      //  event_arr[i] = alloca(64 * sizeof(char));
 
	ret = pfm_get_os_event_encoding(event_name, PFM_PLM0|PFM_PLM3, PFM_OS_NONE, &enc_evt);
	if (ret != PFM_SUCCESS) {
        fprintf(stderr, "Failed to encode event: %s\n", pfm_strerror(ret));
        return NULL;
    }
    
    ret = pfm_get_event_info(enc_evt.idx, PFM_OS_NONE, &info);
    if (ret != PFM_SUCCESS) {
        fprintf(stderr, "cannot get event info: %s", pfm_strerror(ret));
        return NULL;
    }

    uncore_setup(&enc_evt, &info, fstr, event_arr);

    
    vt_plugin_cntr_metric_info *return_values = NULL;
    
    i = 0;
    while(event_arr[i]!=NULL) {
        
        vt_plugin_cntr_metric_info *realloc_tmp=NULL;
        realloc_tmp =  realloc(return_values, (i+2) * sizeof(vt_plugin_cntr_metric_info));
        if (realloc_tmp != NULL) {
            return_values = realloc_tmp;
        }
        else {
            fprintf(stderr,"VT_UNCORE_PLUGIN: realloc failed\n");
            free(return_values);
            return NULL;
        }

        /* if the description is null it should be considered the end */
        return_values[i].name = strdup(event_arr[i]);
        return_values[i].unit = NULL;
        return_values[i].cntr_property = VT_PLUGIN_CNTR_ABS
            | VT_PLUGIN_CNTR_UNSIGNED | VT_PLUGIN_CNTR_LAST;
        /* Last element empty */
        return_values[i+1].name = NULL;
        i++;
    }

    return return_values;
}

void fini(void)
{
    fprintf(stderr, "starting finishing\n");
    counter_enabled = 0;
    
    pthread_join(thread, NULL);

    uncore_fini();

    fprintf(stderr, "finishing done\n");
}


void * thread_report(void * ignore) {
    uint64_t timestamp, timestamp2;
    counter_enabled = 1;

    while (counter_enabled) {
        int i = 0;
        if (wtime == NULL)
            return NULL;
        /* measure time for each msr read */
        for (i = 0; i < event_list_size; i++) {
            if (event_list[i].enabled && !pthread_mutex_trylock(&event_list[i].mutex)) {
                if (event_list[i].result_vector == NULL) {
                    event_list[i].result_vector = malloc(max_data_count * sizeof(vt_plugin_cntr_timevalue));
                    event_list[i].data_count = 0;
                }
                else if (event_list[i].data_count == max_data_count) {
                    fprintf(stderr, "Buffer reached maximum %d. Loosing events.\n", max_data_count);
                    fprintf(stderr, "Set VT_UNCORE_BUF_SIZE environment variable to increase buffer size\n");
                    pthread_mutex_unlock(&event_list[i].mutex);
                    continue;
                }

                /* measure time and read value */
                timestamp = wtime();
                event_list[i].result_vector[event_list[i].data_count].value = read_uncore_box(i);
                timestamp2 = wtime();
                event_list[i].result_vector[event_list[i].data_count].timestamp =
                    timestamp + ((timestamp2 - timestamp) >> 1);

                event_list[i].data_count++;
                pthread_mutex_unlock(&event_list[i].mutex);
            }
        }

        usleep(interval_us);
    }
    return NULL;
}

int32_t add_counter(char * event_name)
{
    int i,j;
    
    /* sort the events by package id */
    if (!is_sorted) {
        struct event tmp;
        for (i=event_list_size;i>1;i--) {
            for (j=0; j<i-1; j++) {
                if (event_list[j].node > event_list[j+1].node) {
                    memcpy(&tmp, &event_list[j], sizeof(struct event));
                    memcpy(&event_list[j], &event_list[j+1], sizeof(struct event));
                    memcpy(&event_list[j+1], &tmp, sizeof(struct event));
                }
            }
        }
        is_sorted=1;
    }

    if (!is_thread_created) {
        pthread_create(&thread, NULL, &thread_report, NULL);
        is_thread_created=1;
    }

    for (i=0;i<event_list_size;i++) {
        if (!strcmp(event_name, event_list[i].name))
            return i;
    }
    return -1;
}

int enable_counter(int ID) {
    event_list[ID].enabled = 1;
    return 0;
}

int disable_counter(int ID) {
    event_list[ID].enabled = 0;
    return 0;
}

uint64_t get_all_values(int32_t id, vt_plugin_cntr_timevalue **result) {
    pthread_mutex_lock(&event_list[id].mutex);
//    fprintf(stderr, "getting values for counter %d\n", id);

	*result = event_list[id].result_vector;

    event_list[id].result_vector = NULL;

    pthread_mutex_unlock(&event_list[id].mutex);
	return event_list[id].data_count;
}

vt_plugin_cntr_info get_info(void)
{
    vt_plugin_cntr_info info;
    memset(&info,0,sizeof(vt_plugin_cntr_info));
    info.init                     = init;
    info.add_counter              = add_counter;
    info.enable_counter           = enable_counter;
    info.disable_counter          = disable_counter;
    info.set_pform_wtime_function = set_pform_wtime_function;
    info.vt_plugin_cntr_version   = VT_PLUGIN_CNTR_VERSION;
    info.run_per                  = VT_PLUGIN_CNTR_PER_HOST;
    info.synch                    = VT_PLUGIN_CNTR_ASYNCH_EVENT;
    info.get_event_info           = get_event_info;
	info.get_all_values           = get_all_values;
    info.finalize                 = fini;
    return info;
}
