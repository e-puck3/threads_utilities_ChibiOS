
# Source files for the Threads utilities library
THDUSRC = $(THDULIB)/threads_utilities.c

# Include directories
THDUINC = $(THDULIB)

# Whether to use or not the threads timestamps feature
USE_THREADS_TIMESTAMPS ?= false

# If enabled, it will use 4bytes * THREADS_TIMESTAMPS_LOG_SIZE of memory
THREADS_TIMESTAMPS_LOG_SIZE ?= 3000
# 
THREADS_TIMESTAMPS_DEFAULT_LOG ?= false

ifeq ($(USE_THREADS_TIMESTAMPS), true)
	ALLDEFS += -DENABLE_THREADS_TIMESTAMPS
	ALLDEFS += -DTHREADS_TIMESTAMPS_LOG_SIZE=$(THREADS_TIMESTAMPS_LOG_SIZE)
	ALLDEFS += -DTHREADS_TIMESTAMPS_DEFAULT_LOG=$(THREADS_TIMESTAMPS_DEFAULT_LOG)
endif

ALLCSRC += $(THDUSRC)
ALLINC  += $(THDUINC)
