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
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of
 * conditions
 *    and the following disclaimer in the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to
 * endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <perfmon/perf_event.h>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>

#include "uncore_perf_plugin.h"
#ifdef X86_ADAPT
#include "x86a_wrapper.h"
#include <x86_adapt.h>
#endif

static pthread_t threads[MAX_EVENTS] = { 0 };
static int thread_enabled[MAX_EVENTS] = { 0 };
static int is_thread_created = 0;
static char vt_sep = '#';
static int ht_enabled;
static struct event event_list[MAX_EVENTS] = { 0 };
static int32_t event_list_size;

static uint64_t (*wtime)(void) = NULL;

#define DEFAULT_BUF_SIZE (size_t)(4 * 1024 * 1024)
static size_t buf_size = DEFAULT_BUF_SIZE; // 4MB per Event per Thread
static int interval_us = 100000;           // 100ms

char* env(const char* name)
{
    int name_len = strlen(name);
    int scorep_len = strlen("SCOREP_METRIC_");
    char* scorep_name = malloc((name_len + scorep_len) * sizeof(char));
    char* ret = getenv(scorep_name);
    if (ret == NULL)
    {
        ret = getenv(name);
    }
    return ret;
}
void set_pform_wtime_function(uint64_t (*pform_wtime)(void))
{
    wtime = pform_wtime;
}

/* PERF_FLAG_FD_CLOEXEC closes the perf event, when the file descriptor is closed */
static inline int sys_perf_event_open(struct perf_event_attr* attr, int cpu)
{
    return syscall(__NR_perf_event_open, attr, -1, cpu, -1, PERF_FLAG_FD_CLOEXEC);
}

static size_t parse_buffer_size(const char* s)
{
    char* tmp = NULL;
    size_t size;

    // parse number part
    size = strtoll(s, &tmp, 10);

    if (size == 0)
    {
        fprintf(stderr, "Failed to parse buffer size ('%s'), using default %zu\n", s,
                DEFAULT_BUF_SIZE);
        return DEFAULT_BUF_SIZE;
    }

    // skip whitespace characters
    while (*tmp == ' ')
        tmp++;

    switch (*tmp)
    {
    case 'G':
        size *= 1024;
    // fall through
    case 'M':
        size *= 1024;
    // fall through
    case 'K':
        size *= 1024;
    // fall through
    default:
        break;
    }

    return size;
}

/* hyperthreading is disabled if if the thread_siblings_list is only 2 bytes, 0 and EOF */
static int is_ht_enabled(void)
{
    int fd = open("/sys/devices/system/cpu/cpu0/topology/thread_siblings_list", O_RDONLY);
    char buf[64] = { 0 };
    if (fd < 0)
    {
        return -1;
    }
    ssize_t count = read(fd, buf, 64);
    switch (count)
    {
    case 0:
        return -1;
    case 2:
        return 0;
    default:
        return 1;
    }
}

/**
 * Returns the number of nodes in this system.
 * NOTE: On AMD_fam15h you should use get_nr_packages() from x86_energy_source.
 */
static int x86_energy_get_nr_packages(void)
{
    int n, nr_packages = 0;
    struct dirent** namelist;
    char* path = "/sys/devices/system/node";

    n = scandir(path, &namelist, NULL, alphasort);
    while (n--)
    {
        if (!strncmp(namelist[n]->d_name, "node", 4))
        {
            nr_packages++;
        }
        free(namelist[n]);
    }
    free(namelist);

    return nr_packages;
}

/**
 * Returns the correponding node of the give cpu.
 * If the cpu can not be found on this system -1 is returned.
 */
static int x86_energy_node_of_cpu(int __cpu)
{
    int nr_packages = x86_energy_get_nr_packages();
    char path[64];

    for (int node = 0; node < nr_packages; node++)
    {
        int sz = snprintf(path, sizeof(path), "/sys/devices/system/node/node%d/cpu%d", node, __cpu);
        assert(sz < sizeof(path));
        struct stat stat_buf;
        int stat_ret = stat(path, &stat_buf);
        if (0 == stat_ret)
        {
            return node;
        }
    }
    return -1;
}

int32_t init(void)
{
    char* env_string;
    int ret;

    /* get number of packages */
    node_num = x86_energy_get_nr_packages();

    env_string = env("UPE_INTERVAL_US");
    if (env_string == NULL)
        interval_us = 100000;
    else
    {
        interval_us = atoi(env_string);
        if (interval_us == 0)
        {
            fprintf(stderr, "Could not parse UPE_INTERVAL_US, using 100 ms\n");
            interval_us = 100000;
        }
    }

    env_string = env("UPE_BUF_SIZE");
    if (env_string != NULL)
    {
        buf_size = parse_buffer_size(env_string);
        if (buf_size < 1024)
        {
            fprintf(stderr, "Given buffer size (%zu) too small, falling back to default (%zu)\n",
                    buf_size, DEFAULT_BUF_SIZE);
            buf_size = DEFAULT_BUF_SIZE;
        }
    }

#if defined(BACKEND_SCOREP)
    env_string = env("UPE_SEP");
    if (env_string != NULL)
    {
        vt_sep = env_string[0];
    }
#endif

    cpus = sysconf(_SC_NPROCESSORS_CONF);
    ht_enabled = is_ht_enabled();
    if (ht_enabled == -1)
    {
        fprintf(stderr, "could not determine if hyperthreading is enabled\n");
        return -1;
    }

    ret = pfm_initialize();
    if (ret != PFM_SUCCESS)
    {
        fprintf(stderr, "cannot initialize library: %s\n", pfm_strerror(ret));
        return -1;
    }

#ifdef X86_ADAPT
    ret = x86a_wrapper_init();
    if (ret)
    {
        fprintf(stderr, "cannot initialize x86 adapt wrapper\n");
        return -1;
    }
#endif

    return 0;
}

static int32_t get_scatter_id(char* event_name)
{
    static int32_t scatter_id = 0;
    int32_t phys_cpus; /* physical cpus per node */
    if (ht_enabled)
    {
        phys_cpus = cpus / 2 / node_num;
    }
    else
    {
        phys_cpus = cpus / node_num;
    }
    /* wrap around scatter id */
    scatter_id = scatter_id % phys_cpus;
    /* assign cbox event to correponding core */
    char* unc_cbo = strstr(event_name, "unc_cbo");
    if (unc_cbo != NULL)
    {
        return atoi(unc_cbo + strlen("unc_cbo"));
    }
    return scatter_id++;
}

metric_properties_t* get_event_info(char* __event_name)
{
    int ret;
    char* fstr = NULL;
    char buf[1024];
    char* event_name = strdup(__event_name);
    int32_t scatter_id;

#ifdef X86_ADAPT
    pfm_pmu_encode_arg_t enc = { 0 };
#else
    pfm_perf_encode_arg_t enc = { 0 };
    struct perf_event_attr attr = { 0 };
    enc.attr = &attr;
#endif
    enc.fstr = &fstr;

#ifdef BACKEND_VTRACE
    for (int i = 0; i < strlen(event_name); i++)
        if (event_name[i] == vt_sep)
            event_name[i] = ':';
#endif

#ifdef X86_ADAPT
    ret = pfm_get_os_event_encoding(event_name, PFM_PLM0 | PFM_PLM3, PFM_OS_NONE, &enc);
#else
    ret = pfm_get_os_event_encoding(event_name, PFM_PLM0 | PFM_PLM3, PFM_OS_PERF_EVENT, &enc);
#endif
    if (ret != PFM_SUCCESS)
    {
        fprintf(stderr, "Failed to encode event: %s\n", event_name);
        fprintf(stderr, "%s\n", pfm_strerror(ret));
        return NULL;
    }

    scatter_id = get_scatter_id(event_name);

    for (int node = 0; node < node_num; node++)
    {
        /* create event name */
        event_list[event_list_size].node = node;
        event_list[event_list_size].scatter_id = scatter_id;
        sprintf(buf, "Package: %d Event: %s", node, fstr);
        event_list[event_list_size].name = strdup(buf);

        event_list[event_list_size].data_count = 0;
        event_list[event_list_size].result_vector = malloc(buf_size);
        if (event_list[event_list_size].result_vector == NULL)
        {
            fprintf(stderr, "Could not allocate memory for result_vector\n");
            return NULL;
        }

        /* we search for the nth cpu on the node to distribute the sampling overhead across multiple
         * cpus */
        int32_t node_cpu = 0;
        for (int i = 0; i < cpus; i++)
        {
            if (x86_energy_node_of_cpu(i) == node)
            {
                if (node_cpu == event_list[event_list_size].scatter_id)
                {
                    event_list[event_list_size].cpu = i;
#ifdef X86_ADAPT
                    int32_t ret = x86a_setup_counter(&(event_list[event_list_size]), &enc, i);
                    if (ret)
                    {
                        fprintf(stderr, "Failed to set up the counter\n");
                        return NULL;
                    }
#else
                    int fd = sys_perf_event_open(&attr, node_cpu);
                    if (fd < 0)
                    {
                        fprintf(stderr, "Failed to get file descriptor\n");
                        return NULL;
                    }
                    event_list[event_list_size].fd = fd;
#endif
                    break;
                }
                else
                {
                    node_cpu++;
                }
            }
        }
        event_list_size++;
    }

    metric_properties_t* return_values = malloc((node_num + 1) * sizeof(metric_properties_t));

    if (return_values == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for information data structure\n");
        return NULL;
    }

    for (int i = 0; i < node_num; i++)
    {
        /* if the description is null it should be considered the end */
        return_values[i].name = strdup(event_list[event_list_size - node_num + i].name);
        return_values[i].unit = NULL;
#ifdef BACKEND_SCOREP
        return_values[i].description = NULL;
        return_values[i].mode = SCOREP_METRIC_MODE_ACCUMULATED_START;
        return_values[i].value_type = SCOREP_METRIC_VALUE_UINT64;
        return_values[i].base = SCOREP_METRIC_BASE_DECIMAL;
        return_values[i].exponent = 0;
#endif
#ifdef BACKEND_VTRACE
        return_values[i].cntr_property =
            VT_PLUGIN_CNTR_ACC | VT_PLUGIN_CNTR_UNSIGNED | VT_PLUGIN_CNTR_LAST;
#endif
    }
    /* Last element empty */
    return_values[node_num].name = NULL;

    return return_values;
}

void fini(void)
{
    int was_enabled[MAX_EVENTS] = { 0 };

    /* disable and join threads */
    for (int i = 0; i < MAX_EVENTS; i++)
    {
        was_enabled[i] = thread_enabled[i];
        thread_enabled[i] = 0;
    }
    for (int i = 0; i < MAX_EVENTS; i++)
    {
        if (was_enabled[i])
        {
            pthread_join(threads[i], NULL);
        }
    }

    for (int i = 0; i < event_list_size; i++)
    {
        close(event_list[i].fd);
        free(event_list[i].name);
    }

#ifdef X86_ADAPT
    x86a_wrapper_fini();
#endif
}

static inline uint64_t uncore_perf_read(struct event* evt)
{
    uint64_t data;
    ssize_t ret;
#ifdef X86_ADAPT
    ret = x86_adapt_get_setting(evt->fd, evt->item, &data);
    if (!ret)
    {
        fprintf(stderr, "Error while reading event %s\n", evt->name);
        return 0;
    }
#else
    ret = read(evt->fd, &data, sizeof(data));
    if (ret != sizeof(data))
    {
        fprintf(stderr, "Error while reading event %s\n", evt->name);
        fprintf(stderr, "%s\n", strerror(errno));
        return 0;
    }
#endif
    return data;
}

#ifndef METRIC_SYNC
static inline uint64_t get_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_usec + tv.tv_sec * 1000000;
}

void* thread_report(void* _cpu)
{
    int32_t cpu = (int32_t)_cpu;
    uint64_t timestamp, timestamp2;
    uint64_t time_in_us, time_next_us = 0;
    size_t num_buf_elems = buf_size / sizeof(timevalue_t);
    struct event* local_event[MAX_EVENTS] = { 0 };
    int32_t local_event_size = 0;

    /* pin thread to cpu */
    cpu_set_t cpu_mask;
    CPU_ZERO(&cpu_mask);
    CPU_SET(cpu, &cpu_mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask);

    /* copy local events */
    for (int i = 0; i < event_list_size; i++)
    {
        if (event_list[i].cpu == cpu)
        {
            local_event[local_event_size] = &(event_list[i]);
            local_event_size++;
        }
    }

    while (thread_enabled[cpu])
    {
        if (wtime == NULL)
            return NULL;
        /* measure time for each msr read */
        for (int i = 0; i < local_event_size; i++)
        {
            if (local_event[i]->enabled)
            {
                if (local_event[i]->data_count >= num_buf_elems)
                {
                    local_event[i]->enabled = 0;
                    fprintf(stderr, "Buffer reached maximum %zuB. Loosing events.\n", (buf_size));
                    fprintf(stderr,
                            "Set UPE_BUF_SIZE environment variable to increase buffer size\n");
                }
                else
                {
                    /* measure time and read value */
                    timestamp = wtime();
                    local_event[i]->result_vector[local_event[i]->data_count].value =
                        uncore_perf_read(local_event[i]);
                    timestamp2 = wtime();
                    local_event[i]->result_vector[local_event[i]->data_count].timestamp =
                        timestamp + ((timestamp2 - timestamp) >> 1);
                    local_event[i]->data_count++;
                }
            }
        }
        time_in_us = get_time();
        time_next_us = time_in_us + interval_us - time_in_us % (interval_us);
        usleep(time_next_us - time_in_us);
    }
    return NULL;
}
#endif

int32_t add_counter(char* event_name)
{
#ifdef X86_ADAPT
    static int32_t once = 0;
    if (!once)
    {
        int32_t ret = x86a_unfreeze_all();
        if (ret)
        {
            return ret;
        }
        once = 1;
    }
#endif
#ifndef METRIC_SYNC
    if (!is_thread_created)
    {
        for (int i = 0; i < event_list_size; i++)
        {
            int cpu = event_list[i].cpu;
            if (!thread_enabled[cpu])
            {
                thread_enabled[cpu] = 1;
                if (pthread_create(&(threads[cpu]), NULL, &thread_report, (void*)cpu) != 0)
                {
                    fprintf(stderr, "Failed to create sampling thread\n");
                    return -1;
                }
            }
        }
        is_thread_created = 1;
    }
#endif

    for (int i = 0; i < event_list_size; i++)
    {
        if (!strcmp(event_name, event_list[i].name))
        {
#ifndef X86_ADAPT
            ioctl(event_list[i].fd, PERF_EVENT_IOC_RESET, 0);
            ioctl(event_list[i].fd, PERF_EVENT_IOC_ENABLE, 0);
#endif
            event_list[i].enabled = 1;
            return i;
        }
    }
    return -1;
}

int enable_counter(int ID)
{
    event_list[ID].enabled = 1;
    return 0;
}

int disable_counter(int ID)
{
    event_list[ID].enabled = 0;
    return 0;
}

#ifdef METRIC_SYNC
bool get_optional_value(int32_t id, uint64_t* value)
{
    if (sched_getcpu() == event_list[id].cpu)
    {
        *value = uncore_perf_read(&event_list[id]);
        return true;
    }
    return false;
}
#else
uint64_t get_all_values(int32_t id, timevalue_t** result)
{
    event_list[id].enabled = 0;

    *result = event_list[id].result_vector;

    return event_list[id].data_count;
}
#endif

#ifdef BACKEND_SCOREP
SCOREP_METRIC_PLUGIN_ENTRY(upe_plugin)
#endif
#ifdef BACKEND_VTRACE
vt_plugin_cntr_info get_info()
#endif
{
    plugin_info_type info = { 0 };
#ifdef BACKEND_SCOREP
    info.plugin_version = SCOREP_METRIC_PLUGIN_VERSION;
    info.initialize = init;
#ifdef METRIC_SYNC
    info.run_per = SCOREP_METRIC_PER_THREAD;
    info.sync = SCOREP_METRIC_SYNC;
#else
    info.run_per = SCOREP_METRIC_PER_HOST;
    info.sync = SCOREP_METRIC_ASYNC;
    info.delta_t = UINT64_MAX;
    info.set_clock_function = set_pform_wtime_function;
#endif
#endif

#ifdef BACKEND_VTRACE
    info.init = init;
    info.vt_plugin_cntr_version = VT_PLUGIN_CNTR_VERSION;
    info.run_per = VT_PLUGIN_CNTR_PER_HOST;
    info.synch = VT_PLUGIN_CNTR_ASYNCH_POST_MORTEM;
    info.set_pform_wtime_function = set_pform_wtime_function;
#endif
    info.add_counter = add_counter;
    info.get_event_info = get_event_info;
#ifdef METRIC_SYNC
    info.get_optional_value = get_optional_value;
#else
    info.get_all_values = get_all_values;
#endif
    info.finalize = fini;
    return info;
}
