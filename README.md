threads_utilities_ChibiOS
=========================
Small library containing several functions to obtain statistics about the threads running like the stack usage or the MCU usage. Another feature is to record the timestamps when ChibiOS does a context switch. Depending on the size allocated for this, a timeline can then be drawn with a python script which gets the data through the USB Shell of ChibiOS.

How to add the library to a project
-----------------------------------

To include the library to your ChibiOS project, simply follow the steps below.

1. Add this git as a submodule (or as a simple folder) to your project.
2. Add the specific variables below to your makefile before the **CSRC** definition
	-	``THDULIB``						: Path to the **threads_utilities_ChibiOS** folder you just added (relative to the ChibiOS makefile)
	-	``USE_THREADS_TIMESTAMPS``		: yes or no. Tells if you want to enable the Timestamps functionality
	-	``THREADS_TIMESTAMPS_LOG_SIZE``	: Size of the logs for the Timestamps functionality
	-	``include $(THDULIB)/threads_utilities.mk``
3. Add these variables in your makefile if it's not already the case
	-	``$(ALLCSRC)`` 	to 	``CSRC``
	-	``$(ALLINC)`` 	to 	``INCDIR``
	-	``$(ALLDEFS)`` 	to 	``UDEFS``

For example with this folder structure :
```
Your_project/
├───threads_utilities_ChibiOS/
│   ├──threads_utilities.mk
│   └──...
├───another_submodule/
└───src/
    ├──makefile
    ├──main.c
    └──...
```
You would add these lines to your makefile :

```makefile
...


THDULIB = ../threads_utilities_ChibiOS
USE_THREADS_TIMESTAMPS = yes
THREADS_TIMESTAMPS_LOG_SIZE = 3000
include $(THDULIB)/threads_utilities.mk

...

# C sources that can be compiled in ARM or THUMB mode depending on the global
# setting.
CSRC =  $(ALLCSRC) \
        ...        \
        ...

...

INCDIR = $(ALLINC) \
         ...       \
         ...

# List all user C define here, like -D_DEBUG=1
UDEFS = $(ALLDEFS) \
        ...        \
        ...

...
```

Versions
--------
In this repository, you can find two main versions of the library (two branches) named **ChibiOS_3.1** and **ChibiOS_18.2**. There are two versions simply because some big changes have been done between these two versions of ChibiOS and we needed to have this library working with the [e-puck2 code](https://github.com/e-puck2/e-puck2_main-processor).
That said it has only been tested with these specific versions of ChibiOS. Newer versions may work with the **ChibiOS_18.2** though.

Timestamps functionality
-----------------------

The Timestamps functionality records each time the system enters or leaves a thread.
These times are stored in two tables of uint32_t which let us record up to 32 threads.

The size used in memory equals (4+4) * ``THREADS_TIMESTAMPS_LOG_SIZE`` bytes.

It's up to you to choose the size of the logs, which directly corresponds to the system ticks. Indeed, each element of the tables corresponds to a system tick. 
A size of 3000 with a frequency of 1000Hz would give 3 sec of records for example.

We can then recover these data and show them on a timeline with the provided Python 3 script findable in **/Python/plot_threads_timeline.py**

### Prerequisite C

1. The recording of the timestamps uses the Hooks possibilities of ChibiOS. Thus, it is necessary to add some lines to the **chconf.h** file of your project as followed.

Add the following lines before the ``#define CH_CFG_CONTEXT_SWITCH_HOOK(ntp, otp)`` hook

```c
#define TIMESTAMPS_INCLUDE
#include "threads_utilities_chconf.h"
```

and the following inside the ``#define CH_CFG_CONTEXT_SWITCH_HOOK(ntp, otp)`` hook

```c
fillThreadsTimestamps(ntp, otp);
```
Don't forget the ``\`` at the end of the lines inside the hook

Here is an example of a correct **chconf.h** file 

```c
#define TIMESTAMPS_INCLUDE
#include "threads_utilities_chconf.h"

/**
 * @brief   Context switch hook.
 * @details This hook is invoked just before switching between threads.
 */
#define CH_CFG_CONTEXT_SWITCH_HOOK(ntp, otp) {                              \
        /* Context switch code here.*/                                            \
        fillThreadsTimestamps(ntp, otp);                                        \
}
```


To be able to send the data, we use the standard USB Shell functionality of ChibiOS. You can find a lot of examples of its use in the **testhal** folder of ChibiOS. They contain the usbcfg.c/.h files to configure the USB and the initialization of the Shell.
When you have a shell working, you need to add the following line in the commands of the Shell to add the thread_utilities commands:

2. Add ``THREADS_UTILITIES_SHELL_CMD`` inside the ``ShellCommand`` array.

For example the following ``ShellCommand`` array : 

```c
...
static const ShellCommand commands[] = {
  {"mem", cmd_mem},
  {"threads", cmd_threads},
  {"test", cmd_test},
  {"write", cmd_write},
  {NULL, NULL}
};
...
```
Would become :

```c
...
static const ShellCommand commands[] = {
  {"mem", cmd_mem},
  {"threads", cmd_threads},
  {"test", cmd_test},
  {"write", cmd_write},
  THREADS_UTILITIES_SHELL_CMD
  {NULL, NULL}
};
...
```
3. Don't forget to include ``threads_utilities.h`` in the files that use a **threads_utilities** function

### Usage Python3

To use the python script, **python3**, as well as **matplotlib** and **pySerial** should be installed.

Then, if the Shell works correctly on the MCU side, you can simply launch the script with the following lines :
 ```
 python3 ./plot_threads_timeline.py ComPort
```

With ``ComPort`` being the USB com port to which the Shell is connected.

Then with the matplotlib window opened, it is possible to use the navigation tools (bottom left) to zoom and move inside the timeline. 
``Left`` and ``Right`` arrows act respectively like Undo and Redo buttons for the view and pressing ``x``or ``y`` while zooming changes the zoom selection to respectively zoom only in the **X** or **Y** axis.

Also, don't forget to let the time to your code to fill the timestamps logs before reading them with the script. If you have 3 seconds of logs, then wait at least 3 seconds before launching the script. Note that once the logs are full, they are kept and no more modified. Of course, nothing prevents you from using the script while the logs are filling. It will work without problem, the only difference being that the next time you will launch the script you will have more data. This can be useful for example if you want to see the effect of the USB communication on the threads.

Keep in mind that this is just a tool to visualize the threads behavior through time. Since most of the threads accomplish small tasks regularly faster than the system tick of ChibiOS, it will often be possible to see multiple threads started on the same timestamp. This means we can't know the real duration of a thread when it's shorter than the timestamp. For readability, the minimal duration drawn by the script is 0,5 system ticks. 
Another thing made to improve the readability in the script is to draw **RED BARS** when a thread has been stopped and started on the same timestamp.

Finally, depending on the zoom level, a lot of information are not visible until you zoom in enough to make them drawable by matplotlib.
