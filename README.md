# Uncore Performance Events Counter Plugin

> *Note:*
>
> This is a plugin for counting performance events asynchronously on every CPU package per system.
> Additionally the plugin offers a synchronous mode under the assumptions that the threads are pinned.
> The plugin was initially created to count uncore performance events but it can be used for every
> event exported by papi\_native\_avail.
> Optionally x86_adapt can be used to count uncore performance events instead of perf.

## Compilation and Installation

### Prerequisites

To compile this plugin, you need:

* GCC compiler

* `libpthread`

* PAPI (`5.4.3+` compiled with --enable-perf-event-uncore)

* Score-P (`1.4+`)

* Linux Kernel 3.10+ (Haswell EP Support 3.16+)

Optional:

* x86_adapt (https://github.com/tud-zih-energy/x86_adapt)

### Building

1. Create a build directory

        mkdir build
        cd build

2. Invoke CMake

    Specify the path to the PAPI headers with `-DPFM_INC` if they're not in the default path.
    Some additional PAPI headers are required, which are not installed by default.
    You need to copy the following header files to your PAPI include directory:

        <papi src dir>/src/libpfm4/include

    Afterwards, just call CMake, e.g.:

        cmake ..

    Alternatively these path can be passed to CMake via `-DPFM_INC`. e.g.:

        cmake .. -DPFM_INC=~/papi/src/libpfm4/include

    Optionally x86_adapt can be used with the `-DX86_ADAPT` CMake flag.

    For compiling the plugin with synchronous mode add the `-DMETRIC_SYNC` CMake flag.

3. Invoke make

        make

4. Copy the resulting `libupe_plugin.so` to a location listed in `LD_LIBRARY_PATH` or add the
    current path to `LD_LIBRARY_PATH` with

        export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`

## Usage

This plugin uses PAPI for parsing the events and utilizes perf for counting uncore performance
events. For counting uncore performance events, privileged rights or reducing the paranoid level is
required, e.g.

    sudo sysctl kernel.perf_event_paranoid=0

The list of available events can be obtained by running `papi_native_avail`. To use this plugin, it
has to be added to the `SCOREP_METRIC_PLUGINS` variable. Afterwards, the events to be counted need
to be added to the `SCOREP_METRIC_UPE_PLUGIN` environment variable, e.g.

    export SCOREP_METRIC_PLUGINS="upe_plugin"
    export SCOREP_METRIC_UPE_PLUGIN="hswep_unc_pcu::UNC_P_CLOCKTICKS"

If you're facing an error message like

    Failed to encode event: hswep_unc_qpi0::UNC_Q_TXR_BL_NCS_CREDIT_ACQUIRED
    invalid or missing unit mask
    [Score-P] src/services/metric/scorep_metric_plugins.c:522: Fatal: Bug 'metric_infos == NULL': Error while initializing plugin metric hswep_unc_qpi0::UNC_Q_TXR_BL_NCS_CREDIT_ACQUIRED, no info returned

you probably missed a needed argument for the specific counter (in this example `:VN0` or `:VN1` has
to be appended to the end of the counter name).

### Environment variables

* `UPE_INTERVAL_US` (default=100000)

    The interval in usecs between two reads of the register.

    A higher interval means less disturbance, a lower interval is more exact. The registers are
    updated roughly every msec. If you choose your interval to be around 1ms you might find highly
    variating power consumptions.

    To gain most exact values, you should set the interval to 10, if you can live with less
    precision, you should set it to 10000.

* `UPE_BUF_SIZE` (default=4194304 (4Mib))

    The size of the buffer for storing elements. A lower size means lesser overhead. But a to small
    buffer might be not capable of storing all events. If this is the case, then a error message
    will be printed to `stderr`.

### If anything fails

1. Check whether the plugin library can be loaded from the `LD_LIBRARY_PATH`.

2. Check with `sysctl` whether `kernel.perf\_event\_paranoid` is 0

3. Write a mail to the author.

## Author

* Michael Werner (michael.werner3 at tu-dresden dot de)
