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

