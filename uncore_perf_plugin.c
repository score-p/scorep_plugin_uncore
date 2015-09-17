/*
   libUPP.so,
   a library to count Uncore events on a SandyBridge E processor and others for Score-P.
   Copyright (C) 2015 TU Dresden, ZIH
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>
#include <perfmon/perf_event.h>

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
    int32_t enabled;
    void * ID;
    size_t data_count;
    timevalue_t *result_vector; 
    char * name;
    int fd;
};

struct event event_list[512];
int event_list_size = 0;
int node_num;
int cpus;

static pthread_t thread;
static int counter_enabled;
static int is_thread_created = 0;
static int is_sorted = 0;
static char vt_sep = '#';

static uint64_t (*wtime)(void) = NULL;

#define DEFAULT_BUF_SIZE (size_t)(4*1024*1024)
static size_t buf_size = DEFAULT_BUF_SIZE; // 4MB per Event per Thread
static int interval_us = 100000; //100ms

void set_pform_wtime_function(uint64_t(*pform_wtime)(void)) {
    wtime = pform_wtime;
}

/* PERF_FLAG_FD_CLOEXEC closes the perf event, when the file descriptor is closed */
static inline int sys_perf_event_open(struct perf_event_attr *attr, int cpu) {
    return syscall(__NR_perf_event_open, attr, -1, cpu, -1, PERF_FLAG_FD_CLOEXEC);
}

static size_t parse_buffer_size(const char *s) {
	char *tmp = NULL;
	size_t size;

	// parse number part
	size = strtoll(s, &tmp, 10);

	if (size == 0) {
        fprintf(stderr, "Failed to parse buffer size ('%s'), using default %zu\n", s, DEFAULT_BUF_SIZE);
        return DEFAULT_BUF_SIZE;
    }

    // skip whitespace characters
    while(*tmp == ' ') tmp++;

    switch(*tmp) {
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

/**
 * Returns the number of nodes in this system.
 * NOTE: On AMD_fam15h you should use get_nr_packages() from x86_energy_source.
 */
static int x86_energy_get_nr_packages(void) {
    int n, nr_packages = 0;
    struct dirent **namelist;
    char *path = "/sys/devices/system/node";

    n = scandir(path, &namelist, NULL, alphasort);
    while(n--) {
        if(!strncmp(namelist[n]->d_name, "node", 4)) {
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
static int x86_energy_node_of_cpu(int __cpu) {
    int node;
    int nr_packages = x86_energy_get_nr_packages();
    char path[64];

    for(node = 0; node < nr_packages; node++)
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

int32_t init(void) {
    char * env_string;
    int ret;

    /* get number of packages */
    node_num = x86_energy_get_nr_packages();

    env_string = getenv("UPP_INTERVAL_US");
    if (env_string == NULL)
        interval_us = 100000;
    else {
        interval_us = atoi(env_string);
        if (interval_us == 0) {
            fprintf(stderr, "Could not parse UPP_INTERVAL_US, using 100 ms\n");
            interval_us = 100000;
        }
    }

    env_string = getenv("UPP_BUF_SIZE");
    if (env_string != NULL) {
        buf_size = parse_buffer_size(env_string);
        if (buf_size < 1024) {
            fprintf(stderr, "Given buffer size (%zu) too small, falling back to default (%zu)\n", buf_size, DEFAULT_BUF_SIZE);
            buf_size = DEFAULT_BUF_SIZE;
        }
    }

#if defined(BACKEND_SCOREP)
    env_string = getenv("UPP_SEP");
    if (env_string != NULL) {
        vt_sep = env_string[0];
    }
#endif

    cpus = sysconf(_SC_NPROCESSORS_CONF);

	ret = pfm_initialize();
	if (ret != PFM_SUCCESS) {
        fprintf(stderr, "cannot initialize library: %s\n", pfm_strerror(ret));
        return -1;
    }

    return 0;
}

metric_properties_t * get_event_info(char * __event_name)
{
    int ret;
    int i;
    int node;
    char * fstr = NULL;
    char buf[1024];
    char * event_name = strdup(__event_name);

    pfm_perf_encode_arg_t enc;
    struct perf_event_attr attr;
    memset(&enc, 0, sizeof(enc));
    memset(&attr, 0, sizeof(attr));
    enc.attr = &attr;
    enc.fstr = &fstr;

#ifdef BACKEND_VTRACE
    for (i=0; i<strlen(event_name);i++) 
        if (event_name[i] == vt_sep)
            event_name[i] = ':';
#endif

    ret = pfm_get_os_event_encoding(event_name, PFM_PLM0 | PFM_PLM3, PFM_OS_PERF_EVENT, &enc);
	if (ret != PFM_SUCCESS) {
        fprintf(stderr, "Failed to encode event: %s\n", event_name);
        fprintf(stderr, "%s\n", pfm_strerror(ret));
        return NULL;
    }
    
    for (node=0;node<node_num;node++) {
        /* create event name */
        event_list[event_list_size].node = node;
        sprintf(buf, "Package: %d Event: %s", node, fstr);
        event_list[event_list_size].name = strdup(buf);

        event_list[event_list_size].data_count = 0;
        event_list[event_list_size].result_vector = malloc(buf_size);
        if (event_list[event_list_size].result_vector == NULL) {
            fprintf(stderr, "Could not allocate memory for result_vector\n");
            return NULL;
        }

        for(i=0;i<cpus;i++) {
            if(x86_energy_node_of_cpu(i) == node) {
                int fd = sys_perf_event_open(&attr, i);
                if (fd < 0) {
                    fprintf(stderr, "Failed to get file descriptor\n");
                    return NULL;
                }
                event_list[event_list_size].fd = fd;
                break;
            }
        }
        event_list_size++;
    }

    metric_properties_t *return_values = malloc((node_num+1) * sizeof(metric_properties_t));

    if (return_values == NULL) {
      fprintf(stderr, "Failed to allocate memory for information data structure\n");
      return NULL;
    }

    for (i=0;i<node_num;i++) {
    /* if the description is null it should be considered the end */
        return_values[i].name = strdup(event_list[event_list_size - node_num + i].name);
        return_values[i].unit = NULL;
#ifdef BACKEND_SCOREP
        return_values[i].description = NULL;
        return_values[i].mode        = SCOREP_METRIC_MODE_ACCUMULATED_LAST;
        return_values[i].mode        = SCOREP_METRIC_MODE_ACCUMULATED_START;
        return_values[i].value_type  = SCOREP_METRIC_VALUE_UINT64;
        return_values[i].base        = SCOREP_METRIC_BASE_DECIMAL;
        return_values[i].exponent    = 0;
#endif
#ifdef BACKEND_VTRACE
        return_values[i].cntr_property = VT_PLUGIN_CNTR_ACC
            | VT_PLUGIN_CNTR_UNSIGNED | VT_PLUGIN_CNTR_LAST;
#endif
    }
    /* Last element empty */
    return_values[i].name = NULL;

    return return_values;
}

void fini(void) {
    int i;
    fprintf(stderr, "starting finishing\n");
    counter_enabled = 0;
    
    pthread_join(thread, NULL);

    for (i=0;i<event_list_size;i++) {
        close(event_list[i].fd);
        free(event_list[i].name);
    }

    fprintf(stderr, "finishing done\n");
}

static inline uint64_t uncore_perf_read(int id) {
    uint64_t data;
    ssize_t ret;
    ret = read(event_list[id].fd, &data, sizeof(data));
    if (ret != sizeof(data)) {
        fprintf(stderr, "Error while reading event %s\n", event_list[id].name);
        fprintf(stderr, "%s\n",strerror(errno));
        return 0;
    }

    return data;
}

void * thread_report(void * ignore) {
    uint64_t timestamp, timestamp2;
    size_t num_buf_elems = buf_size/sizeof(timevalue_t);
    counter_enabled = 1;

    while (counter_enabled) {
        int i = 0;
        if (wtime == NULL)
            return NULL;
        /* measure time for each msr read */
        for (i = 0; i < event_list_size; i++) {
            if (event_list[i].enabled) {
                if (event_list[i].data_count >= num_buf_elems) {
                        event_list[i].enabled = 0;
                        fprintf(stderr, "Buffer reached maximum %zuB. Loosing events.\n", (buf_size));
                        fprintf(stderr, "Set UPP_BUF_SIZE environment variable to increase buffer size\n");
                    }
                else {
                    /* measure time and read value */
                    timestamp = wtime();
                    event_list[i].result_vector[event_list[i].data_count].value = uncore_perf_read(i);
                    timestamp2 = wtime();
                    event_list[i].result_vector[event_list[i].data_count].timestamp =
                        timestamp + ((timestamp2 - timestamp) >> 1);
                    event_list[i].data_count++;
                }
            }
        }
        usleep(interval_us);
    }
    return NULL;
}

int32_t add_counter(char * event_name) {
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
        if (!strcmp(event_name, event_list[i].name)) {
            ioctl(event_list[i].fd, PERF_EVENT_IOC_RESET, 0);
            ioctl(event_list[i].fd, PERF_EVENT_IOC_ENABLE, 0);
            // Todo needed?
            event_list[i].enabled = 1;
            return i;
        }
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

uint64_t get_all_values(int32_t id, timevalue_t **result) {
    counter_enabled = 0;
    pthread_join(thread, NULL);

	*result = event_list[id].result_vector;

	return event_list[id].data_count;
}

#ifdef BACKEND_SCOREP
SCOREP_METRIC_PLUGIN_ENTRY( UPP )
#endif
#ifdef BACKEND_VTRACE
vt_plugin_cntr_info get_info()
#endif
{
    plugin_info_type info;
    memset(&info,0,sizeof(plugin_info_type));
#ifdef BACKEND_SCOREP
    info.plugin_version               = SCOREP_METRIC_PLUGIN_VERSION;
    info.run_per                      = SCOREP_METRIC_PER_HOST;
    info.sync                         = SCOREP_METRIC_ASYNC;
    info.delta_t                      = UINT64_MAX;
    info.initialize                   = init;
    info.set_clock_function           = set_pform_wtime_function;
#endif

#ifdef BACKEND_VTRACE
    info.init                     = init;
    info.vt_plugin_cntr_version   = VT_PLUGIN_CNTR_VERSION;
    info.run_per                  = VT_PLUGIN_CNTR_PER_HOST;
    info.synch                    = VT_PLUGIN_CNTR_ASYNCH_POST_MORTEM;
    info.set_pform_wtime_function = set_pform_wtime_function;
#endif
    info.add_counter              = add_counter;
    info.get_event_info           = get_event_info;
    info.get_all_values           = get_all_values;
    info.finalize                 = fini;
    return info;
}
