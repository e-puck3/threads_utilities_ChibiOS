/**
 * @file	uc_usage.h
 * @brief  	Functions to show the uC usage
 * 
 * @source			http://www.chibios.com/forum/viewtopic.php?f=2&t=138&start=10
 * @modified by  	Eliot Ferragni
 */

#ifndef UC_USAGE_H
#define UC_USAGE_H

#include "ch.h"
#include "chprintf.h"

// size of the buffers used for the threads timeline functionality
// will take 4+4 bytes * THREADS_TIMELINE_LOG_SIZE
#define THREADS_TIMELINE_LOG_SIZE	3000

/**
 * @brief 			Prints the uC usage on the output specified
 * 					The values returned have a relatively big inertia
 * 					because ChibiOS counts the total time used by each thread.
 * 
 * @param device 	Pointer to the output
 */	
void printUcUsage(BaseSequentialStream* out);

/**
 * @brief 			Prints the number of threads running
 * 
 * @param device 	Pointer to the output
 */	
void printStatThreads(BaseSequentialStream *out);

/**
 * @brief 			Saves the in and out times of each running thread while the buffers aren't full.
 * 					To be called by the CH_CFG_CONTEXT_SWITCH_HOOK in chconf.h
 * 
 * @param device 	Pointer to the output
 */	
void fillThreadsTimeline(void* ntp, void* otp);

/**
 * @brief 			Prints the number of threads running
 * 
 * @param device 	Pointer to the output
 */	
void printCountThreads(BaseSequentialStream *out);

/**
 * @brief 			Prints the timeline of the threads, aka the in and out times
 * 					of each running thread that has been recorded by the function fillThreadsTimeline().
 * 					Made to work in pair with plot_threads_timeline.py to generate a timeline graph which 
 * 					let us visualize when a thread is running.
 * 					
 * 					Better to call via the USB shell since the python script will send a specific command
 * 
 * @param device 	Pointer to the output
 */	
void printTimelineThread(BaseSequentialStream *out, uint8_t thread_number);

#endif /* UC_USAGE_H */