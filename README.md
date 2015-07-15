#Uncore Performance Events Counter Plugin

> *Note:*
>
> This a plugin for counting uncore performance events on Intel Westermere, SandyBridge E and AMD
> Interlagos microarchitecture.
>
> For uncore cbox counters the SandyBridge E CBOX synchronous plugin should be used.

##Compilation and Installation

###Prerequisites

To compile this plugin, you need:

* GCC compiler

* `libpthread`

* `libx86_energy`

* PAPI (`5.2+`)

* VampirTrace (`5.14+`)

* The kernel module `msr` should be active (you might use `modprobe`) and you should have readwrite
    access to `/dev/cpu/*/msr`

> *Note:*
>
> For accessing northbridge register e.g. on SandyBridge root access is necessary. On the other
> microarchitectures access to the msr registers is sufficient.

###Building

1. Create a build directory

        mkdir build
        cd build

2. Invoke CMake

    Specify the `x86_energy` directory if it is not in the default path with `-DX86_ENERGY_INC=<PATH>`.

    CMake and make will be invoked on the `x86_energy` lib. To use a prebuild `x86_energy` lib use
    `-DX86_ENERGY_BUILD=OFF` and specify the path with `-DX86_ENERGY_LIB=<PATH>`.

    Default is to link static. To link dynamic turn `-DX86_ENERGY_STATIC=OFF`.

        cmake ..

    Example for a prebuild static linked `x86_energy` which is not in the default path:

        cmake .. -DX86_ENERGY_INC=~/X86_ENERGY_energylib -DX86_ENERGY_LIB=~/X86_ENERGY_energylib/build -DX86_ENERGY_BUILD=OFF

3. Invoke make

        make

4. Copy it to a location listed in `LD_LIBRARY_PATH` or add current path to `LD_LIBRARY_PATH` with

        export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`

> *Note:*
>
> If `libx86` energy ist dynamic linked then the path to `libx86_energy.so` has to be in
> `LD_LIBRARY_PATH` too

##Usage

This plugin uses the event parsing api from `libpfm`. `libpfm` supplies the neat tool `showevtinfo`
to display all supported events.

Examples:

    export VT_PLUGIN_CNTR_METRICS=UNCOREPlugin_UNC_H_CLOCKTICKS

If there are multiple pmu you have to supply them explicitly.

    export VT_PLUGIN_CNTR_METRICS=UNCOREPlugin_snbep_unc_imc0##UNC_M_CAS_COUNT:UNCOREPlugin_snbep_unc_imc1##UNC_M_CAS_COUNT

###Environment variables

* `VT_UNCORE_INTERVAL_US` (default=100000)

    The interval in usecs, the register is read.

    A higher interval means less disturbance, a lower interval is more exact. The registers are
    updated roughly every msec. If you choose your interval to be around 1ms you might find highly
    variating power consumptions.

    To gain most exact values, you should set the interval to 10, if you can live with less
    precision, you should set it to 10000.

* `VT_UNCORE_BUF_SIZE` (default=100)

    The size of the buffer for storing elements. A lower size means leasser overhead. But a to small
    buffer might be not capable of storing all events. If this is the case, then a error message
    will be printed to `stderr`.

###If anything fails

1. Check whether the plugin library can be loaded from the `LD_LIBRARY_PATH`.

2. Check whether you are allowed to read `/dev/cpu/*/msr`

3. Write a mail to the author.

##Author

* Michael Werner (michael.werner3 at tu-dresden dot de)
