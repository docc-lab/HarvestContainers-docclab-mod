# HarvestMonitor - Debug Code

## Overview

`HarvestMonitor` is a Linux kernel module that checks for CPU idle status by continually calling the *idle_cpu()* Linux kernel function. It provides access to the current number of idle CPUs and a mask showing CPU idle/active status via shared memory.

## Building

*idle_cpu()* is not exported to modules by stock kernels. The module no longer requires a patched kernel: at load time it resolves *idle_cpu()* through *kallsyms_lookup_name()*, whose address it obtains with a kprobe. The running kernel therefore needs `CONFIG_KPROBES=y` and `CONFIG_KALLSYMS=y` (both enabled in Ubuntu and mainline configs). If the lookup fails, `insmod` fails and `dmesg` says why.

The module was originally developed with Linux 5.6 (where `EXPORT_SYMBOL(idle_cpu);` had to be added to `./kernel/sched/core.c`). It has since been ported to 6.x kernels (`task_is_running()`/`task_cpu()`, single-argument `class_create()` on 6.4+, no `MODULE_SUPPORTED_DEVICE`) and was verified to build cleanly against 6.14 headers.

Use the included `Makefile` to build, with the headers of the running kernel installed (`/lib/modules/$(uname -r)/build`).

The cpulogger buffer is allocated with `vmalloc` at load time and holds `logsize` entries of 24 bytes each (default 60M entries, ~1.4 GB). Override it at load time if needed, e.g. `sudo insmod qidlecpu.ko logsize=10000000`.

## Running

1. Insert the module by issuing command `sudo insmod qidlecpu.ko`
2. On load, the module will create a character device at `/dev/qidlecpu` and a procfs mount at `/proc/idlecpu`. The device is used for shared memory between kernel/userspace and the procfs mount is primarily used to start/stop *idle_cpu()* polling.
3. The module's polling loop is stopped by default. Start it by issuing command `echo 1 > /proc/idlecpu/control` and stop it by issuing command `echo 0 > /proc/idlecpu/control`.
4. See the `HarvestBalancer` code for an example of how to mount shared memory exported by `HarvestMonitor` for sharing idle CPU status.

## Caveats

There are some rough edges. Things to keep in mind:

1. To avoid locking up the system by backlogging too many RCUs, the module calls *schedule()* every 100,000 iterations. This works in theory and seems to work in practice, but there may be (and likely is) a more elegant solution.
