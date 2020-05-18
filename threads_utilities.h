/**
 * @file	threads_utilities.h
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
 * @brief 			Prints a list of the declared threads
 * 
 * @param device 	Pointer to the output
 */	
void printListThreads(BaseSequentialStream *out);

/**
 * @brief 			Prints the timestamps of the threads, aka the IN and OUT times
 * 					of each running thread that has been recorded by the function fillThreadsTimestamps().
 * 					Made to work in pair with plot_threads_timeline.py to generate a timeline graph which 
 * 					let us visualize when a thread is running.
 * 					
 * 					Better to call via the USB shell since the python script will send a specific command
 * 
 * @param device 	Pointer to the output
 */	
void printTimestampsThread(BaseSequentialStream *out);

/**     
 * @brief 			The thread invocking this functions will be logged in the timestamps functionality
 * 
 */	
void logThisThreadTimestamps(void);

/**     
 * @brief 			The thread invocking this functions will no longer be logged in the timestamps functionality
 * 					If it was logged before, it will appear as a non logged thread in the python script (the representation is different)
 * 
 */	
void dontLogThisThreadTimestamps(void);

/**     
 * @brief 			All the following threads to be created will be logged in the timestamps functionnality
 * 					Can be called before chSysInit() to log the main and Idle threads for example
 * 					Works in pair with dontLogNextCreatedThreadsTimestamps() to log some threads but not all
 * 
 */	
void logNextCreatedThreadsTimestamps(void);

/**     
 * @brief 			All the following threads to be created won't be logged in the timestamps functionality
 * 					Can be called before chSysInit() to not log the main and Idle threads for example
 * 					Works in pair with logNextCreatedThreadsTimestamps() to log some threads but not all
 * 
 */	
void dontLogNextCreatedThreadsTimestamps(void);

/**     
 * @brief 			Sets the trigger of the timestamps functionality. This means the filling is stopped
 * 					such that the timestamps are equally around the trigger.
 * 
 */	
const char* setTriggerTimestamps(const char* trigger_name);

/**     
 * @brief 			Removes the trigger of the timestamps functionality to be in run mode. This means 
 * 					the continuous filling works again.
 * 
 */	
void resetTriggerTimestamps(void);

/********************                SHELL FUNCTIONS               ********************/

/**     
 * @brief 			Shell command to print the threads declared
 * 					Calls printListThreads()
 */	
void cmd_threads_list(BaseSequentialStream *chp, int argc, char *argv[]);
/**     
 * @brief 			Sets the trigger
 * 					Calls setTriggerTimestamps()
 */	
void cmd_threads_timestamps_trigger(BaseSequentialStream *chp, int argc, char *argv[]);
/**     
 * @brief 			Goes into run mode
 * 					Calls resetTriggerTimestamps()
 */	
void cmd_threads_timestamps_run(BaseSequentialStream *chp, int argc, char *argv[]);
/**     
 * @brief 			Shell command to print the number of threads running
 * 					Calls printTimestampsThread()
 */	
void cmd_threads_timestamps(BaseSequentialStream *chp, int argc, char *argv[]);
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
	{"threads_list",cmd_threads_list},			\
	{"threads_timestamps",cmd_threads_timestamps},		\
	{"threads_timestamps_trigger", cmd_threads_timestamps_trigger}, \
	{"threads_timestamps_run", cmd_threads_timestamps_run}, \
	{"threads_stat", cmd_threads_stat},				\
	{"threads_uc", cmd_threads_uc},					\


#endif /* UC_USAGE_H */