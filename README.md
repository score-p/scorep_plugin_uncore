#Uncore Performance Events Counter Plugin

> *Note:*
>
> This is a plugin for couting performance events asynchronously on every cpu package per system.
> The plugin was initially created to count uncore performance events but it can be used for every
> event exported by papi\_native\_avail.

##Compilation and Installation

###Prerequisites

To compile this plugin, you need:

* GCC compiler

* `libpthread`

* PAPI (`5.4+`)

* Score-P (`1.4+`)

* Linux Kernel 3.10+ (Haswell EP Support 3.16+)


###Building

1. Create a build directory

        mkdir build
        cd build

2. Invoke CMake

    Specify the path to the PAPI headers with `-DPAPI_INC` if they're not in the default path.
    Some additional papi headers are required, which are not installed by default.
    You need to copy the following header files to your papi include directory:

        <papi src dir>/src/libpfm4/include

    Afterwards, just call CMake, e.g.:

        cmake ..

3. Invoke make

        make

4. Copy the resulting `libupp_plugin.so` to a location listed in `LD_LIBRARY_PATH` or add the
    current path to `LD_LIBRARY_PATH` with

        export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`

##Usage

This plugin uses papi for parsing the events and utilises perf for counting uncore performance
events. For couting uncore performance events, priviliged rights or reducing the paranoid level is
required, e.g.

    sudo sysctl kernel.perf_event_paranoid=0

The list of available events can be obtained by running `papi_native_avail`. To use this plugin, it
has to be added to the `SCOREP_METRIC_PLUGINS` variable. Afterwards, the events to be counted need
to be added to the `SCOREP_METRIC_UPP_PLUGIN` environment variable, e.g.

    export SCOREP_METRIC_PLUGINS="upp_plugin"
    export SCOREP_METRIC_UPP_PLUGIN="hswep_unc_pcu::UNC_P_CLOCKTICKS"

If you're facing an error message like

    Failed to encode event: hswep_unc_qpi0::UNC_Q_TXR_BL_NCS_CREDIT_ACQUIRED
    invalid or missing unit mask
    [Score-P] src/services/metric/scorep_metric_plugins.c:522: Fatal: Bug 'metric_infos == NULL': Error while initializing plugin metric hswep_unc_qpi0::UNC_Q_TXR_BL_NCS_CREDIT_ACQUIRED, no info returned

you probably missed a needed argument for the specific counter (in this example `:VN0` or `:VN1` has
to be appended to the end of the counter name).

###Environment variables

* `UPP_INTERVAL_US` (default=100000)

    The interval in usecs between two reads of the register.

    A higher interval means less disturbance, a lower interval is more exact. The registers are
    updated roughly every msec. If you choose your interval to be around 1ms you might find highly
    variating power consumptions.

    To gain most exact values, you should set the interval to 10, if you can live with less
    precision, you should set it to 10000.

* `UPP_BUF_SIZE` (default=4194304 (4Mib))

    The size of the buffer for storing elements. A lower size means leasser overhead. But a to small
    buffer might be not capable of storing all events. If this is the case, then a error message
    will be printed to `stderr`.

###If anything fails

1. Check whether the plugin library can be loaded from the `LD_LIBRARY_PATH`.

2. Check with `sysctl` whether `kernel.perf\_event\_paranoid` is 0

3. Write a mail to the author.

##Author

* Michael Werner (michael.werner3 at tu-dresden dot de)
