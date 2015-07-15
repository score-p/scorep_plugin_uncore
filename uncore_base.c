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
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <x86_energy.h>

#include "uncore_boxes.h"
#include "uncore_base.h"
#include "uncore_plugin.h"

static uint64_t read_register(int cpu, uint32_t reg){
    int  fd;
    uint64_t data;
    char msr_file_name[64];

    sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
    fd = open(msr_file_name, O_RDONLY);
    if (fd < 0)
    {
        printf( "UNCORE_PLUGIN: ERROR\n");
        printf( "UNCORE_PLUGIN: rdmsr: failed to open '%s'!\n",msr_file_name);
        exit(fd);
    }

    if (pread(fd, &data, sizeof data, reg) != sizeof data)
    {
        printf( "UNCORE_PLUGIN: ERROR\n");
        printf( "UNCORE_PLUGIN: rdmsr: failed to read '%s': register %x!\n",msr_file_name , reg);
        exit(-1);
    }
    close(fd);
    return data;
}

static void write_register(int cpu, uint32_t reg, uint64_t data) {
    int  fd;
    char msr_file_name[64];

    sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
    fd = open(msr_file_name, O_WRONLY);

    if (fd < 0)
    {
        printf( "UNCORE_PLUGIN: ERROR: could not open %s for writing.\n", msr_file_name);
    }

    if (pwrite(fd, &data, sizeof data, reg) != sizeof data)
    {
        printf( "UNCORE_PLUGIN: ERROR\n");
        printf( "UNCORE_PLUGIN: wrmsr: failed to write '%s': register %x!\n",msr_file_name , reg);
        exit(-1);
    }

    close(fd);
}

static uint32_t read_nb_register(int fd, uint32_t reg) {
    uint32_t data;
    int ret =  pread(fd, &data, sizeof data, reg);
    if ( ret != sizeof data)
    {
        printf( "UNCORE_PLUGIN: ERROR\n");
        printf( "UNCORE_PLUGIN: read northbridge: failed to read register %x!\n", reg);
        printf("size of pread %d\n",  ret );
        exit(-1);
    }
    return data;
}

static void write_nb_register(int fd, uint32_t reg, uint32_t data) {
    int ret = pwrite(fd, &data, sizeof data, reg);
    if (ret != sizeof data)
    {
        printf( "UNCORE_PLUGIN: ERROR\n");
        printf("size of pwrite %d\n",  ret);
        printf( "UNCORE_PLUGIN: write northbridgeg: failed to write register %x!\n", reg);
        exit(-1);
    }
}

static inline int open_pci(char *a, char *b) {
    char buf[30];
    strcpy(buf, a);
    strcat(buf, b);
    return open(buf, O_RDWR);
}

static inline char* get_pci(int nb) {
    if (node_num == 4) {
        switch(nb) {
            case 0: return "/proc/bus/pci/3f/";
            case 1: return "/proc/bus/pci/7f/";
            case 2: return "/proc/bus/pci/bf/";
            case 3: return "/proc/bus/pci/ff/";
        }
    } else {
        switch(nb) {
            case 0: return "/proc/bus/pci/3f/";
            case 1: return "/proc/bus/pci/7f/";
        }
    }
    return NULL;
}

uint64_t read_uncore_box(int id) {

    uint64_t value, old;

    old = event_list[id].last;
    /* read msr or read pci */
    if (event_list[id].box->type == MSR) {
        read_msr(&event_list[id].handle);
        value = event_list[id].handle.data;
    }
    else {
        value = read_nb_register(event_list[id].fd, event_list[id].reg) |
              (((uint64_t)(read_nb_register(event_list[id].fd, event_list[id].reg+0x4))<<32));
    }
  
    event_list[id].last = value;

    if (!event_list[id].init) {
        event_list[id].init=1;
        return 0;
    }

    if (old>value) {
        fprintf(stderr, "overflow on node %d\n", event_list[id].node);
        return event_list[id].box->max-old+value;
    }
    else
        return value-old;
}

int uncore_init(void) {
    int i, node, fd;

    if(init_msr(O_RDWR))
        return -1;

    /* reset pci */
    for (node=0;node<node_num;node++) {
        /* sandy bridge */
        for_each_snbep_box(i) {
            /* only reset pci counter, when uncore box counter is not NULL */
            if (snbep_uncore_boxes[i].type == PCI) {
                fd = open_pci(get_pci(node), snbep_uncore_boxes[i].counter);
                if (fd < 0) {
                    fprintf(stderr, "UNCORE_PLUGIN: failed to reset counter %s\n",
                    pmu_to_str(snbep_uncore_boxes[i].pmu));
                    return -1;
                }
                write_nb_register(fd, 0xf4, 0x3);
                close(fd);
            }
        }
        /* westmere reset */
        if (pmu_is_present(PFM_PMU_INTEL_WSM_UNC)) {
            for (i=0;i<cpus;i++) {
                if (x86_energy_node_of_cpu(i) == node) {
                    write_register(i, WSM_UNCORE_PERF_GLOBAL_CTRL, 0);
                    break;
                }
            }
        }
        /* reset for amd fam15h is not needed */
    }
    

    return 0;
}

void uncore_fini(void) {
    int i;
    for(i=0;i<event_list_size;i++) {
        if (event_list[i].box->type == MSR) {
            close_msr(event_list[i].handle);
        }
        else { /* pci */
            close(event_list[i].fd);
        }
        pthread_mutex_destroy(&event_list[i].mutex);
    }
}

int uncore_setup(pfm_pmu_encode_arg_t* enc_evt, pfm_event_info_t* info,
    char * event_name, char** ret_arr)
{
    struct uncore_box * box;
    uint32_t config = 0;
    uint64_t config1 = 0;
    int i, node;
    char buf[1024];
    
    if (!pmu_is_present(info->pmu)) {
        fprintf(stderr, "PMU %s is not present\n", pmu_to_str(info->pmu));
        return -1;
    }

    if (pmu_to_box(info->pmu, &box)) {
        fprintf(stderr, "PMU %s is not supported by this plugin\n", 
            pmu_to_str(info->pmu));
        return -1;
    }

    /* check for counter overload */
    if (box->curr == box->max_cntr) {
        fprintf(stderr, "VT_UNCORE_PLUGIN: To many events for PMU %s\n",
            pmu_to_str(info->pmu));
        return -1;
    }
    
    config = enc_evt->codes[0];
    /* enable counting */
    if (arch_of(info->pmu) == INTEL_SANDYBRIDGE_EP ||
        arch_of(info->pmu) == AMD_FAM15h)
        config |= 1<<22; 
    if (enc_evt->count == 2)
        config1 = enc_evt->codes[1];
    
    fprintf(stderr, "parsed event %x\n", config);

    /* create event on each package */
    for(node=0;node<node_num;node++) {
        /* create event records */
        event_list[event_list_size].node = node;
        event_list[event_list_size].box = box;
        sprintf(buf, "Package: %d Event: %s", node, event_name);
        event_list[event_list_size].name = strdup(buf);
        ret_arr[node] = event_list[event_list_size].name;

        /* initialize mutex */
        pthread_mutex_init(&event_list[event_list_size].mutex, NULL);
        event_list[event_list_size].data_count = 0;
        event_list[event_list_size].result_vector = NULL;

        /* set and open counter */
        /* msr */
        if(event_list[event_list_size].box->type == MSR) {
            for(i=0;i<cpus;i++) {
                if(x86_energy_node_of_cpu(i) == node) {
                    write_register(i, box->cfg[box->curr], config);    
                    open_msr(i, box->reg[box->curr], &event_list[event_list_size].handle); 

                    /* special case intel snbep pcu */
                    if (event_list[event_list_size].box->pmu == PFM_PMU_INTEL_SNBEP_UNC_PCU) {
                        config1 |= read_register(i, 0xc34);
                        write_register(i, 0xc34, config1);
                    }

                    /* special case for intel westmere */
                    if (arch_of(info->pmu) == INTEL_WESTMERE) {
                        uint64_t tmp = read_register(i, WSM_UNCORE_PERF_GLOBAL_CTRL);
                        tmp |= 0x1 << box->curr;
                        write_register(i, WSM_UNCORE_PERF_GLOBAL_CTRL, tmp);
                    }
                    
                        

                    break;
                }
            }
        }
        /* pci */
        else {
            int fd = open_pci(get_pci(node), box->counter);
            if(fd < 0){
                fprintf(stderr,"UNCORE_PLUGIN: failed to open fd %s \n", 
                pmu_to_str(box->pmu));
            }
            event_list[event_list_size].fd = fd;
            event_list[event_list_size].reg = box->reg[box->curr];
            write_nb_register(fd, box->cfg[box->curr], config);
        }

        event_list_size++;
    }

    box->curr++;

    /* null termination */
    ret_arr[node] = NULL;
    return 0;
}

int pmu_to_box(pfm_pmu_t pmu, struct uncore_box ** box)
{
    int i;
    /* sandy bridge */
    for_each_snbep_box(i) {
        if (snbep_uncore_boxes[i].pmu == pmu) {
            *box = &snbep_uncore_boxes[i];
            return 0;
        }
    }
    /* westmere */
    for_each_wsm_box(i) {
        if (wsm_uncore_boxes[i].pmu == pmu) {
            *box = &wsm_uncore_boxes[i];
            return 0;
        }
    }
    /* amd bulldozer */
    for_each_fam15h_box(i) {
        if (fam15h_uncore_boxes[i].pmu == pmu) {
            *box = &fam15h_uncore_boxes[i];
            return 0;
        }
    }
    
    return -1;
} 

int pmu_is_present(pfm_pmu_t pmu)
{
    int ret;
    pfm_pmu_info_t pinfo;

	memset(&pinfo, 0, sizeof(pinfo));
    ret = pfm_get_pmu_info(pmu, &pinfo);
    if (ret) {
		fprintf(stderr, "Failed to get pmu info: %s\n", pfm_strerror(ret));
        exit(-1);
    }
    return pinfo.is_present;
}

int arch_of(pfm_pmu_t pmu) 
{
    switch (pmu) {
        case PFM_PMU_INTEL_WSM_UNC:
            return INTEL_WESTMERE;
        case PFM_PMU_INTEL_SNBEP_UNC_HA:
        case PFM_PMU_INTEL_SNBEP_UNC_IMC0:
        case PFM_PMU_INTEL_SNBEP_UNC_IMC1:
        case PFM_PMU_INTEL_SNBEP_UNC_IMC2:
        case PFM_PMU_INTEL_SNBEP_UNC_IMC3:
        case PFM_PMU_INTEL_SNBEP_UNC_PCU:
        case PFM_PMU_INTEL_SNBEP_UNC_QPI0:
        case PFM_PMU_INTEL_SNBEP_UNC_QPI1:
        case PFM_PMU_INTEL_SNBEP_UNC_R2PCIE:
        case PFM_PMU_INTEL_SNBEP_UNC_R3QPI0:
        case PFM_PMU_INTEL_SNBEP_UNC_R3QPI1:
        case PFM_PMU_INTEL_SNBEP_UNC_UBOX:
            return INTEL_SANDYBRIDGE_EP;
        case PFM_PMU_AMD64_FAM15H_INTERLAGOS:
            return AMD_FAM15h;
        default:
            return ARCH_UNSUPPORTED;
    }
}


const char * pmu_to_str(pfm_pmu_t pmu)
{
    int ret;
    pfm_pmu_info_t pinfo;

	memset(&pinfo, 0, sizeof(pinfo));
    ret = pfm_get_pmu_info(pmu, &pinfo);
    if (ret) {
		fprintf(stderr, "Failed to get pmu info: %s\n", pfm_strerror(ret));
        exit(-1);
    }
    return pinfo.name;
}
