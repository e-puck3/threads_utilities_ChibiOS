/**
 * @file	threads_utilities_chconf.h
 * @brief  	Functions to obtain various stats about the threads
 * 			
 * 			The timestamps functionality works a bit like what you would expect from an oscilloscope.
 * 			The IN and OUT times of the threads are recorded continuously in run mode (no trigger set)
 * 			and when a trigger is set, either by software or by a USB command through the shell,
 * 			the recording of the IN and OUT is stopped such that we have an equal number of timestamps
 * 			before and after the trigger.
 * 			
 * 			The python3 script "plot_threads_timeline.py" is provided to draw a timeline to visualize the data.
 * 
 * @source			http://www.chibios.com/forum/viewtopic.php?f=2&t=138&start=10
 * @source2			http://www.chibios.com/forum/viewtopic.php?t=4496
 * @created by  	Eliot Ferragni
 * @last modif		25 may 2020
 */

#ifndef THREADS_UTILITIES_CHCONF_H
#define THREADS_UTILITIES_CHCONF_H

// To be added to CH_CFG_THREAD_EXTRA_FIELDS
#define TIMESTAMPS_THREAD_EXTRA_FIELDS					                    \
	uint8_t log_this_thread;												\
	thread_t* log_newer_thread;												\
	thread_t* log_older_thread;

// To be added to CH_CFG_THREAD_INIT_HOOK
#define TIMESTAMPS_THREAD_INIT_HOOK						                    \
		tp->log_this_thread = getLogSetting();								\
		addThread(tp);

// To be added to CH_CFG_THREAD_EXIT_HOOK
#define TIMESTAMPS_THREAD_EXIT_HOOK(tp)									    \
        removeThread(tp);
// To be added to CH_CFG_CONTEXT_SWITCH_HOOK
#define TIMESTAMPS_CONTEXT_SWITCH_HOOK(ntp, otp)						    \
        fillThreadsTimestamps(ntp, otp);


#ifdef TIMESTAMPS_INCLUDE

#if !defined(_FROM_ASM_)
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

uint8_t getLogSetting(void);
void fillThreadsTimestamps(void* in, void* out);
void removeThread(void* otp);
void addThread(void* ntp);
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _FROM_ASM_ */

#undef TIMESTAMPS_INCLUDE
#endif /* TIMESTAMPS_INCLUDE */

#endif /* THREADS_UTILITIES_CHCONF_H */