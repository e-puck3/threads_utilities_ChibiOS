
# Source files for the Threads utilities library
THDUSRC = $(THDULIB)/threads_utilities.c

# Include directories
THDUINC = $(THDULIB)

# Whether to use or not the threads timestamps feature
USE_THREADS_TIMESTAMPS ?= no

# If enabled, it will use 8bytes * THREADS_TIMESTAMPS_LOG_SIZE of memory
# Each element corresponds to a system tick
THREADS_TIMESTAMPS_LOG_SIZE ?= 3000

ifeq ($(USE_THREADS_TIMESTAMPS),yes)
	ALLDEFS += -DENABLE_THREADS_TIMESTAMPS
	ALLDEFS += -DTHREADS_TIMESTAMPS_LOG_SIZE=$(THREADS_TIMESTAMPS_LOG_SIZE)
	ALLDEFS += -DTIMESTAMPS_TRIGGER_MODE
endif

ALLCSRC += $(THDUSRC)
ALLINC  += $(THDUINC)
