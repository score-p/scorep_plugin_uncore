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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include <x86_adapt.h>
#include "x86a_wrapper.h"

static int32_t initialized = 0;

regex_t cbo_regex, ha_regex, imc_regex, qpi_regex, r3qpi_regex, sbo_regex;


#define comp_boxreg(box) \
    do { \
        int32_t ret = regcomp(& box ## _regex, "unc_"#box"([[:digit:]]+)", REG_EXTENDED); \
        if (ret) { \
            fprintf(stderr, "failed to compile regex for box "#box); \
            return ret; \
        } \
    } while(0)

#define check_ptr(ptr)                                                           \
    do {                                                                         \
        if (ptr == NULL) {                                                       \
            fprintf(stderr, "Failed to allocate memory in x86a_wrapper_init\n"); \
            exit(-1);                                                           \
        }                                                                        \
    } while (0)

#define check_return(ret, str) \
    do { \
        if (ret != 8) { \
            fprintf(stderr, str); \
            return -1; \
        } \
    } while (0)

static inline int32_t __lookup(const char* name)
{
    return x86_adapt_lookup_ci_name(X86_ADAPT_DIE, name);
}

#define __FIXED 0
#define __NORM 1

#define __NONE 0
#define __SINGLE 1
#define __MULTI 2

static inline void __lookup_single(struct unc_box* box, int32_t type, char* ctl,
    char* ctr)
{
    struct unc_pair* pair = NULL;

    if (__lookup(ctl) > 0) {
        pair = malloc(sizeof(struct unc_pair));
        check_ptr(pair);
        pair->used = 0;
        pair->ctl = __lookup(ctl);
        pair->ctr = __lookup(ctr);
        if (type == __FIXED)
            box->fixed_size++;
        else
            box->norm_size++;
    }

    if (type == __FIXED)
        box->fixed = pair;
    else
        box->norm = pair;
}

static inline void __lookup_multi(struct unc_box* box, int32_t type, char* _ctl,
    char* _ctr)
{
    struct unc_pair* pair = NULL;
    int32_t i = 0;
    char ctl[64];
    char ctr[64];
    sprintf(ctl, "%s%d", _ctl, i);
    sprintf(ctr, "%s%d", _ctr, i);

    while (__lookup(ctl) > 0) {
        pair = realloc(pair, (i + 1) * sizeof(struct unc_pair));
        check_ptr(pair);
        pair[i].used = 0;
        pair[i].ctl = __lookup(ctl);
        pair[i].ctr = __lookup(ctr);

        i++;
        sprintf(ctl, "%s%d", _ctl, i);
        sprintf(ctr, "%s%d", _ctr, i);
    }

    if (type == __FIXED) {
        box->fixed = pair;
        box->fixed_size = i;
    }
    else {
        box->norm = pair;
        box->norm_size = i;
    }
}

static inline void __duplicate_box(struct unc_box** _box, int32_t size)
{
    struct unc_box* box = NULL;
    if (node_num <= 1)
        return;
    box = realloc(*_box, sizeof(struct unc_box) * size * node_num);
    check_ptr(box);
    for (int32_t i=1; i < node_num; i++) {
        for (int32_t j=0; j < size; j++) {
            int32_t k = i * size + j;
            memcpy(&(box[k]), &(box[j]), sizeof(struct unc_box));
            if (box[k].fixed_size > 0) {
                box[k].fixed = malloc(sizeof(struct unc_pair) * box[k].fixed_size);
                check_ptr(box[k].fixed);
                memcpy(box[k].fixed, box[j].fixed, sizeof(struct unc_pair) * box[j].fixed_size);
            }
            if (box[k].norm_size > 0) {
                box[k].norm = malloc(sizeof(struct unc_pair) * box[k].norm_size);
                check_ptr(box[k].norm);
                memcpy(box[k].norm, box[j].norm, sizeof(struct unc_pair) * box[j].norm_size);
            }
        }
    }
    *_box = box;
}


static inline void __single_box(struct unc_box** _box, int32_t* box_size, char* box_prefix,
    int32_t fixed_type, int32_t norm_type)
{
    struct unc_box* box = NULL;
    char ci_name[64], ctl[64], ctr[64];
    sprintf(ci_name, "%s%s", box_prefix, "_PMON_STATUS");
    *box_size = 0;
    if (__lookup(ci_name) > 0) {
        box = malloc(sizeof(struct unc_box));
        check_ptr(box);
        box->status = __lookup(ci_name);
        sprintf(ci_name, "%s%s", box_prefix, "_PMON_BOX_CTL");
        box->ctl = __lookup(ci_name);
        sprintf(ci_name, "%s%s", box_prefix, "_PMON_FILTER");
        box->filter0 = __lookup(ci_name);
        box->filter1 =
        box->fixed_size = 0;
        box->norm_size = 0;
        sprintf(ctl, "%s%s", box_prefix, "_PMON_FIXED_CTL");
        sprintf(ctr, "%s%s", box_prefix, "_PMON_FIXED_CTR");
        if (fixed_type == __SINGLE)
            __lookup_single(box, __FIXED, ctl, ctr);
        if (fixed_type == __MULTI)
            __lookup_multi(box, __FIXED, ctl, ctr);

        sprintf(ctl, "%s%s", box_prefix, "_PMON_CTL");
        sprintf(ctr, "%s%s", box_prefix, "_PMON_CTR");
        if (norm_type == __SINGLE)
            __lookup_single(box, __NORM, ctl, ctr);
        if (norm_type == __MULTI)
            __lookup_multi(box, __NORM, ctl, ctr);
        *box_size = 1;
        *_box = box;
        __duplicate_box(_box, *box_size);
    }
}


static inline void __multi_box(struct unc_box** _box, int32_t* box_size, char* box_prefix,
    int32_t fixed_type, int32_t norm_type)
{
    int32_t i = 0;
    struct unc_box* box = NULL;
    char ci_name[64], ctl[64], ctr[64];
    sprintf(ci_name, "%s%d%s", box_prefix, i, "_PMON_STATUS");
    while (__lookup(ci_name) > 0) {
        box = realloc(box, (i+1) * sizeof(struct unc_box));
        check_ptr(box);

        box[i].status = __lookup(ci_name);
        sprintf(ci_name, "%s%d%s", box_prefix, i, "_PMON_BOX_CTL");
        box[i].ctl = __lookup(ci_name);
        sprintf(ci_name, "%s%d%s", box_prefix, i, "_PMON_FILTER0");
        box[i].filter0 = __lookup(ci_name);
        sprintf(ci_name, "%s%d%s", box_prefix, i, "_PMON_FILTER1");
        box[i].filter1 = __lookup(ci_name);
        box[i].fixed_size = 0;
        box[i].norm_size = 0;
        sprintf(ctl, "%s%d%s", box_prefix, i, "_PMON_FIXED_CTL");
        sprintf(ctr, "%s%d%s", box_prefix, i, "_PMON_FIXED_CTR");
        if (fixed_type == __SINGLE)
            __lookup_single(&(box[i]), __FIXED, ctl, ctr);
        if (fixed_type == __MULTI)
            __lookup_multi(&(box[i]), __FIXED, ctl, ctr);

        sprintf(ctl, "%s%d%s", box_prefix, i, "_PMON_CTL");
        sprintf(ctr, "%s%d%s", box_prefix, i, "_PMON_CTR");
        if (norm_type == __SINGLE)
            __lookup_single(&(box[i]), __NORM, ctl, ctr);
        if (norm_type == __MULTI)
            __lookup_multi(&(box[i]), __NORM, ctl, ctr);

        i++;
        sprintf(ci_name, "%s%d%s", box_prefix, i, "_PMON_STATUS");
    }
    *_box = box;
    *box_size = i;
    __duplicate_box(_box, *box_size);
}

#define __reset_box(box) \
    do { \
        for (int32_t i=0; i<box ## _size; i++) { \
            ret = x86_adapt_set_setting(all_nodes, box[i].ctl, 0x3); \
            check_return(ret, "Failed to reset " #box "\n"); \
        } \
    } while (0)


int32_t x86a_wrapper_init(void)
{
    int32_t ret;
    if (initialized) {
        return 0;
    }

    if (x86_adapt_init()) {
        fprintf(stderr, "Could not initialize x86_adapt library");
        return -1;
    }

    /* buildup uncore box entries */
    /* global registers */
    global_ctl = __lookup("GLOBAL_PMON_BOX_CTL");
    global_status = __lookup("GLOBAL_PMON_STATUS");
    global_config = __lookup("GLOBAL_PMON_CONFIG");

    /* ubox */
    __single_box(&ubox, &ubox_size, "U", __SINGLE, __MULTI);

    /* pcubox */
    __single_box(&pcubox, &pcubox_size, "PCU", __NONE, __MULTI);

    /* sbox */
    __multi_box(&sbox, &sbox_size, "S", __NONE, __MULTI);
    comp_boxreg(sbo);

    /* cbox */
    __multi_box(&cbox, &cbox_size, "C", __NONE, __MULTI);
    comp_boxreg(cbo);

    /* habox */
    __multi_box(&habox, &habox_size, "HA", __NONE, __MULTI);
    comp_boxreg(ha);

    /* imcbox */
    __multi_box(&imc0box, &imc0box_size, "IMC0_CHAN", __SINGLE, __MULTI);
    __multi_box(&imc1box, &imc1box_size, "IMC1_CHAN", __SINGLE, __MULTI);
    comp_boxreg(imc);

    /* irpbox */
    // NOTE: not available in papi
    __multi_box(&irpbox, &irpbox_size, "IRP", __NONE, __MULTI);
    //comp_boxreg(irp);

    /* qpibox */
    __multi_box(&qpibox, &qpibox_size, "QPI", __NONE, __MULTI);
    comp_boxreg(qpi);

    /* r2pci */
    __single_box(&r2pcibox, &r2pcibox_size, "R2PCIe", __NONE, __MULTI);

    /* r3qpibox */
    __multi_box(&r3qpibox, &r3qpibox_size, "R3QPI0_Link_", __NONE, __MULTI);
    comp_boxreg(r3qpi);

    int32_t all_nodes = x86_adapt_get_all_devices(X86_ADAPT_DIE);
    if (all_nodes < 0) {
        fprintf(stderr, "Could not get fd for resetting the boxes\n");
        return -1;
    }
    /* freeze counter */
    ret = x86_adapt_set_setting(all_nodes, global_ctl, (1u << 31u));
    check_return(ret, "Failed to freeze counter\n");

    /* reset boxes */
    __reset_box(pcubox);
    __reset_box(sbox);
    __reset_box(cbox);
    __reset_box(habox);
    __reset_box(imc0box);
    __reset_box(imc1box);
    __reset_box(irpbox);
    __reset_box(qpibox);
    __reset_box(r2pcibox);
    __reset_box(r3qpibox);

    initialized = 1;

    return x86_adapt_put_all_devices(X86_ADAPT_DIE);
}

static inline void __free_box(struct unc_box *box, int32_t box_size)
{
    if (box_size < 1) {
      return;
    }
    for (int32_t i=0; i < box_size * node_num ; i++) {
        if (box[i].fixed_size > 0) {
          free(box[i].fixed);
        }

        if(box[i].norm_size > 0) {
          free(box[i].norm);
        }
    }
    free(box);
}

void x86a_wrapper_fini(void)
{
    if (!initialized) {
        return;
    }

    __free_box(ubox, ubox_size);
    __free_box(pcubox, pcubox_size);
    __free_box(sbox, sbox_size);
    __free_box(cbox, cbox_size);
    __free_box(habox, habox_size);
    __free_box(imc0box, imc0box_size);
    __free_box(imc1box, imc1box_size);
    __free_box(irpbox, irpbox_size);
    __free_box(qpibox, qpibox_size);
    __free_box(r2pcibox, r2pcibox_size);
    __free_box(r3qpibox, r3qpibox_size);

    regfree(&cbo_regex);
    regfree(&ha_regex);
    regfree(&imc_regex);
    regfree(&qpi_regex);
    regfree(&r3qpi_regex);
    regfree(&sbo_regex);

    x86_adapt_finalize();
    initialized = 0;
}

static inline int32_t __match_box(struct unc_box** ret_box, const char* pfm_name,
                                  regex_t* box_regex, struct unc_box* box, int32_t size,
                                  int32_t node)
{
    int32_t ret;
    regmatch_t pmatch[2];
    char boxnum[8] = {0};
    ret = regexec(box_regex, pfm_name, 2, pmatch, 0);
    if (!ret) {
        strncpy(boxnum, pfm_name+pmatch[1].rm_so, pmatch[1].rm_eo - pmatch[1].rm_so);
        int32_t num = atoll(boxnum);
        /* imcbox corner case handling
         * papi enumerates the channel from both controllers 0-7
         * but adapt enurate the channels for each controller */
        if (strstr(pfm_name, "unc_imc") != NULL) {
        //if (box == NULL) {
            if (num >= 0 && num < imc0box_size) {
                *ret_box = &(imc0box[num + node * size]);
            }
            else if (num >= 4 && num < (imc1box_size + 4)) {
                *ret_box = &(imc1box[num-4 + node * size]);
            }
            return 0;
        }
        /* default case */
        else if (num >= 0 && num < size) {
            *ret_box = &(box[num + node * size]);
            return 0;
        }
        else {
            return -1;
        }
    }
    return ret;
}

static int32_t __get_box(struct unc_box** box, const char* pfm_name, int32_t node)
{
    if (node >= node_num) {
        fprintf(stderr, "Node %d not available\n", node);
        return -1;
    }
    /* single boxes */
    /* ubox */
    if (strstr(pfm_name, "unc_ubo") != NULL && ubox_size > 0) {
        *box = &(ubox[node]);
        return 0;
    }
    /* pcubox */
    if (strstr(pfm_name, "unc_pcu") != NULL && pcubox_size > 0) {
        *box = &(pcubox[node]);
        return 0;
    }
    /* r2pcie */
    if (strstr(pfm_name, "unc_r2pcie") !=NULL && r2pcibox_size > 0) {
        *box = &(r2pcibox[node]);
        return 0;
    }

    /* multi boxes */
    /* cbox */
    if (!__match_box(box, pfm_name, &cbo_regex, cbox, cbox_size, node))
        return 0;

    /* habox */
    if (!__match_box(box, pfm_name, &ha_regex, habox, habox_size, node))
        return 0;

    /* imcbox */
    /* corner case */
    if (!__match_box(box, pfm_name, &imc_regex, NULL, imc0box_size, node))
        return 0;

    /* qpibox */
    if (!__match_box(box, pfm_name, &qpi_regex, qpibox, qpibox_size, node))
        return 0;

    /* r3qpibox */
    if (!__match_box(box, pfm_name, &r3qpi_regex, r3qpibox, r3qpibox_size, node))
        return 0;

    /* sbox */
    if (!__match_box(box, pfm_name, &sbo_regex, sbox, sbox_size, node))
        return 0;

    /* no box found */
    return -1;
}

int32_t x86a_setup_counter(struct event* evt, pfm_pmu_encode_arg_t* enc, int32_t cpu)
{
    struct unc_box* box;
    int32_t ret, ctr = 0, fixed = 0;
    uint64_t data;

    /* corner case for fixed counters */
    /* only happens with imc boxes */
    if (strstr(enc->fstr[0],"unc_imc") != NULL &&
        strstr(enc->fstr[0],"UNC_M_CLOCKTICKS") != NULL)
    {
        fixed = 1;
    }

    ret = __get_box(&box, (const char*) enc->fstr[0], evt->node);
    if (ret) {
        fprintf(stderr, "Failed to retrieve box for event %s on node %d\n", enc->fstr[0], evt->node);
        return -1;
    }

    /* get fd for the device */
    evt->fd = x86_adapt_get_device(X86_ADAPT_DIE, evt->node);
    if (evt->fd < 0) {
        fprintf(stderr, "Failed to get file descriptor on cpu %d\n", cpu);
        return -1;
    }

    /* special case, fixed counter */
    if (fixed) {
        box->fixed[0].used = 1;
        evt->item = box->fixed[0].ctr;
        ret = x86_adapt_set_setting(evt->fd, box->fixed[0].ctl, enc->codes[0] | (1u << 22u) );
        if (ret != 8) {
            fprintf(stderr, "Failed to write counter config for event %s\n", enc->fstr[0]);
            return -1;
        }

        return 0;
    }

    /* normal counter */
    /* first free counter */
    while (ctr < box->norm_size && box->norm[ctr].used)
        ctr++;
    if (ctr >= box->norm_size) {
        fprintf(stderr, "No free counter available for event %s\n", enc->fstr[0]);
        return -1;
    }
    box->norm[ctr].used = 1;
    evt->item = box->norm[ctr].ctr;

    switch (enc->count) {
        case 3:
            data = 0;
            ret = x86_adapt_get_setting(evt->fd, box->filter1, &data);
            if (!ret) {
                fprintf(stderr, "Failed to read filter register 1 for event %s\n", enc->fstr[0]);
                return -1;
            }
            data |= enc->codes[2];
            ret = x86_adapt_set_setting(evt->fd, box->filter1, data);
            if (ret != 8) {
                fprintf(stderr, "Failed to write filter register 1 for event %s\n", enc->fstr[0]);
                return -1;
            }
        case 2:
            data = 0;
            ret = x86_adapt_get_setting(evt->fd, box->filter0, &data);
            if (!ret) {
                fprintf(stderr, "Failed to read filter register 0 for event %s\n", enc->fstr[0]);
                return -1;
            }
            data |= enc->codes[1];
            ret = x86_adapt_set_setting(evt->fd, box->filter0, data);
            if (ret != 8) {
                fprintf(stderr, "Failed to write filter register 0 for event %s\n", enc->fstr[0]);
                return -1;
            }
        case 1:
            ret = x86_adapt_set_setting(evt->fd, box->norm[ctr].ctl, enc->codes[0] | (1u << 22u) );
            if (ret != 8) {
                fprintf(stderr, "Failed to write counter config for event %s\n", enc->fstr[0]);
                return -1;
            }
            break;
        default:
            fprintf(stderr, "Unknown event encode size %d\n",enc->count);
            return -1;
    }
    return 0;
}

int32_t x86a_unfreeze_all(void) {

    int32_t ret;
    int32_t all_nodes = x86_adapt_get_all_devices(X86_ADAPT_DIE);
    if (all_nodes < 0) {
        fprintf(stderr, "Could not get fd for resetting the boxes\n");
        return -1;
    }
    /* freeze counter */
    ret = x86_adapt_set_setting(all_nodes, global_ctl, (1u << 29u));
    check_return(ret, "Failed to unfreeze all counter\n");
    return x86_adapt_put_all_devices(X86_ADAPT_DIE);
}
