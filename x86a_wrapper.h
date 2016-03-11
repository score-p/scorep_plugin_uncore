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
#include <perfmon/pfmlib.h>

#include "uncore_perf_plugin.h"

struct unc_pair {
    int32_t used;
    int32_t ctr;
    int32_t ctl;
};

struct unc_box {
    int32_t status;
    int32_t ctl;
    int32_t filter0;
    int32_t filter1;
    int32_t fixed_size;
    int32_t norm_size;
    struct unc_pair *fixed;
    struct unc_pair *norm;
};

int32_t global_ctl;
int32_t global_status;
int32_t global_config;
struct unc_box *ubox;
int32_t ubox_size;
struct unc_box *pcubox;
int32_t pcubox_size;
struct unc_box *cbox;
int32_t cbox_size;
struct unc_box *sbox;
int32_t sbox_size;
struct unc_box *habox;
int32_t habox_size;
struct unc_box *imc0box;
int32_t imc0box_size;
struct unc_box *imc1box;
int32_t imc1box_size;
struct unc_box *irpbox;
int32_t irpbox_size;
struct unc_box *qpibox;
int32_t qpibox_size;
struct unc_box *r2pcibox;
int32_t r2pcibox_size;
struct unc_box *r3qpibox;
int32_t r3qpibox_size;


int32_t x86a_wrapper_init(void);
void x86a_wrapper_fini(void);
int32_t x86a_setup_counter(struct event*, pfm_pmu_encode_arg_t* enc_evt, int32_t);
int32_t x86a_unfreeze_all(void);

