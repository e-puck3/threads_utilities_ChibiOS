/**
 * @file	threads_utilities.h
 * @brief  	Functions to obtain various stats about the threads
 * 
 * @source			http://www.chibios.com/forum/viewtopic.php?f=2&t=138&start=10
 * @source2			http://www.chibios.com/forum/viewtopic.php?t=4496
 * @created by  	Eliot Ferragni
 * @last modif		9 april 2020
 */

#ifndef UC_USAGE_H
#define UC_USAGE_H

#include "ch.h"
#include "chprintf.h"

/********************                PUBLIC FUNCTIONS              ********************/

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
 * @brief 			Prints the number of threads running
 * 
 * @param device 	Pointer to the output
 */	
void printCountThreads(BaseSequentialStream *out);

/**
 * @brief 			Prints the timeline of the threads, aka the in and out times
 * 					of each running thread that has been recorded by the function fillThreadsTimestamps().
 * 					Made to work in pair with plot_threads_timeline.py to generate a timeline graph which 
 * 					let us visualize when a thread is running.
 * 					
 * 					Better to call via the USB shell since the python script will send a specific command
 * 
 * @param device 	Pointer to the output
 */	
void printTimestampsThread(BaseSequentialStream *out, uint8_t thread_number);

/********************                CHCONF FUNCTION               ********************/

/**
 * @brief 			Saves the in and out times of each running thread while the buffers aren't full.
 * 					To be called by the CH_CFG_CONTEXT_SWITCH_HOOK in chconf.h
 * 
 * @param device 	Pointer to the output
 */	
void fillThreadsTimestamps(void* ntp, void* otp);

/********************                SHELL FUNCTIONS               ********************/

/**     
 * @brief 			Shell command to print the number of threads running
 * 					Calls printCountThreads()
 */	
void cmd_threads_count(BaseSequentialStream *chp, int argc, char *argv[]);
/**     
 * @brief 			Shell command to print the number of threads running
 * 					Calls printTimestampsThread()
 */	
void cmd_threads_timeline(BaseSequentialStream *chp, int argc, char *argv[]);
/**     
 * @brief 			Shell command to print stats abouts the memory usage of the threads
 * 					Calls printStatThreads()
 */	
void cmd_threads_stat(BaseSequentialStream *chp, int argc, char *argv[]);
/**     
 * @brief 			Shell command to print usage of MCU time per threads
 * 					Calls printUcUsage()
 */	
void cmd_threads_uc(BaseSequentialStream *chp, int argc, char *argv[]);

#define THREADS_UTILITIES_SHELL_CMD					\
	{"threads_count",cmd_threads_count},			\
	{"threads_timeline",cmd_threads_timeline},		\
	{"threads_stat", cmd_threads_stat},				\
	{"threads_uc", cmd_threads_uc},					\


#endif /* UC_USAGE_H */