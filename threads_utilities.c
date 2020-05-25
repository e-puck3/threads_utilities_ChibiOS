/**
 * @file	threads_utilities.c
 * @brief  	Functions to obtain various stats about the threads
 * 
 * @source			http://www.chibios.com/forum/viewtopic.php?f=2&t=138&start=10
 * @source2			http://www.chibios.com/forum/viewtopic.php?t=4496
 * @created by  	Eliot Ferragni
 * @last modif		25 may 2020
 */

#include <stdlib.h>
#include "threads_utilities.h"

/********************              INTERNAL VARIABLES              ********************/

// we can store the in and out time of 63 threads max
// each case of the tabs corresponds to 1 system tick
#ifdef ENABLE_THREADS_TIMESTAMPS

#define MAX_REMOVED_THREADS	64

// Whether we automatically log next created threads or not
static uint8_t logSetting = THREADS_TIMESTAMPS_DEFAULT_LOG;

// max 63 Threads (1-63), 0 means nothing
// bit 31 to bit 12 = time (20bits -> 17 minutes max)
// bit 11 to bit 6 	= out thread number (1-63)	
// bit 5  to bit 0 	= in thread number (1-63)
#define THREAD_IN_POS		0
#define THREAD_IN_MASK		(0x3F << THREAD_IN_POS)
#define THREAD_OUT_POS		6
#define THREAD_OUT_MASK		(0x3F << THREAD_OUT_POS)
#define TIME_POS			12
#define TIME_MASK			(0xFFFFF << TIME_POS)
// Circular buffer containing the timestamps logs
static uint32_t _threads_log[THREADS_TIMESTAMPS_LOG_SIZE] = {0};
static uint32_t _fill_pos = 0;
static uint8_t _pause = false;
static uint8_t _full = false;

// Trigger variables
static uint8_t _triggered = false;
static uint32_t _trigger_time = 0;
static int32_t _fill_remaining = 0;

// Mask used to know if we have a thread_t pointer or infos about a deleted dynamic thread
// The value 0xFFFFxxxx is a memory zone not used to store variables so if we set this value, then
// we don't have a thread_t pointer but infos about prio and log for a dynamic thread
#define DYN_THD_REMOVED_MASK 	0xFFFF0000

// bit 31 to bit 16 = FFFF to know if thread_t pointer or the following representation
// bit 15 to bit 8 	= priority (0-255)	
// bit 0		 	= thread logged or not (1 or 0)
#define DYN_THD_PRIO_POS		8
#define DYN_THD_PRIO_MASK		(0xFF << DYN_THD_PRIO_POS)
#define DYN_THD_LOG_POS 		0
#define DYN_THD_LOG_MASK 		(0x01 << DYN_THD_LOG_POS)

// Generic name used if we need to print a deleted dynamic thread
static char generic_dynamic_thread_name[] = {"Exited dynamic thread"};

// Circular list that keeps traces of the exited thread which still have timestamps in the logs
static thread_t* _threads_removed_infos[MAX_REMOVED_THREADS] = {NULL};
static uint8_t _threads_removed[MAX_REMOVED_THREADS] = {0};
static uint8_t _threads_removed_pos = 0;
static uint8_t _next_thread_removed_to_delete = 0;
static uint8_t _threads_removed_count = 0;

// Simple internal linked list of the threads alive
static thread_t* first_added_thread = NULL;
static thread_t* last_added_thread = NULL;

#else
static char no_timestamps_error_message[] = {
	"The thread timestamps functionality is disabled\r\n"
	"Please define USE_THREADS_TIMESTAMPS = true in your makefile \r\n"
};
#endif /* ENABLE_THREADS_TIMESTAMPS */


/********************               PRIVATE FUNCTIONS              ********************/

#ifdef ENABLE_THREADS_TIMESTAMPS
/**     
 * @brief 			Tells whether we can add a timestamp to the logs buffer or not
 */	
uint8_t _continue_to_fill(void){
	if(_triggered){
		if(_fill_remaining <= 0){
			return false;
		}else{
			return true;
		}
	}else{
		if(_pause){
			return false;
		}else{
			return true;
		}
	}
}

/**     
 * @brief 			Updates the log variables when an element has been added to the logs buffer
 */	
void _increments_fill_pos(void){
	_fill_pos++;
	if(_fill_pos >= THREADS_TIMESTAMPS_LOG_SIZE){
		_fill_pos = 0;
		_full = true;
	}
	if(_triggered){
		_fill_remaining--;
	}
}
#endif /* ENABLE_THREADS_TIMESTAMPS */


/********************                CHCONF FUNCTION               ********************/

/**     
 * @brief 			Adds a new thread to the internal thread list
 * 					To be called by the CH_CFG_THREAD_INIT_HOOK() in chconf.h
 * 
 * @param ntp 		Pointer to the new thread to add
 */	
void addThread(void* ntp){

	thread_t* tp = (thread_t*) ntp;
	
	if(first_added_thread == NULL){
		first_added_thread = tp;
	}

	tp->log_newer_thread = NULL;
	tp->log_older_thread = last_added_thread;

	if(tp->log_older_thread != NULL){
		tp->log_older_thread->log_newer_thread = tp;
	}
	last_added_thread = tp;

}

/**
 * @brief 			Saves the exiting thread into the removed thread list to keep trace of the 
 * 					deleted threads for the timestamps functionality. Also removes the thread from the
 * 					internal thread list.
 * 					To be called by the CH_CFG_THREAD_EXIT_HOOK in chconf.h
 * 
 * @param otp 		Pointer to the exiting thread
 */	
void removeThread(void* otp){
#ifdef ENABLE_THREADS_TIMESTAMPS
	static thread_t *tp = NULL;
	static thread_t *out = NULL;
	static uint8_t counter_out = 0;
	static uint32_t time = 0;

	time = chVTGetSystemTimeX();

	// Finds the number of the thread (its place in the list)
	tp = first_added_thread;
	out = (thread_t*) otp;
	counter_out = 1;
	while(tp != NULL){
		if(tp == out){
			break;
		}
		tp = tp->log_newer_thread;
		counter_out++;
	}

	// Adds the thread to the removed list if we can
	if(_threads_removed_count < MAX_REMOVED_THREADS){
		_threads_log[_fill_pos] = ((time << TIME_POS) & TIME_MASK) 
								| ((counter_out << THREAD_OUT_POS) & THREAD_OUT_MASK)
								| ((counter_out << THREAD_IN_POS) & THREAD_IN_MASK);
		_increments_fill_pos();

		_threads_removed[_threads_removed_pos] = counter_out;

		if(out->flags == CH_FLAG_MODE_STATIC){
			_threads_removed_infos[_threads_removed_pos] = out; 

		}else if(out->flags == CH_FLAG_MODE_HEAP || out->flags == CH_FLAG_MODE_MPOOL){
			_threads_removed_infos[_threads_removed_pos] = (thread_t*)	(DYN_THD_REMOVED_MASK 
														 				| ((out->prio << DYN_THD_PRIO_POS) & DYN_THD_PRIO_MASK)
																		| ((out->log_this_thread << DYN_THD_LOG_POS) & DYN_THD_LOG_MASK));
		}

		_threads_removed_pos = (_threads_removed_pos+1) % MAX_REMOVED_THREADS;
		_threads_removed_count++;
	}

	// Removes the thread from the list
	if(out->log_older_thread != NULL){
		out->log_older_thread->log_newer_thread = out->log_newer_thread;
	}else{
		first_added_thread = out->log_newer_thread;
	}

	if(out->log_newer_thread != NULL){
		out->log_newer_thread->log_older_thread = out->log_older_thread;
	}else{
		last_added_thread = out->log_older_thread;
	}
#else
	(void) otp;
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

/**
 * @brief 			Saves the IN and OUT times of each thread to log while the buffers aren't full.
 * 					To be called by the CH_CFG_CONTEXT_SWITCH_HOOK in chconf.h
 * 
 * @param ntp 		Pointer to the IN thread
 * @param otp 		Pointer to the OUT thread
 */	
void fillThreadsTimestamps(void* ntp, void* otp){
#ifdef ENABLE_THREADS_TIMESTAMPS
	static uint32_t time = 0;
	static thread_t *tp = 0;
	static thread_t *in = NULL;
	static thread_t *out = NULL;
	static uint8_t counter = 0;
	static uint8_t counter_in = 0;
	static uint8_t counter_out = 0;
	static uint8_t found_in = false;
	static uint8_t found_out = false;

	time = chVTGetSystemTimeX();

	// Finds the number of the threads (their place in the list)
	if(_continue_to_fill()){
		in = (thread_t*) ntp;
		out = (thread_t*) otp;
		tp = first_added_thread;
		counter = 1;
		counter_in = 0;
		counter_out = 0;
		found_in = false;
		found_out = false;
		while(tp != NULL){
			if(tp == in){
				found_in = true;
				counter_in = counter;
			}
			if(tp == out){
				found_out = true;
				counter_out = counter;
			}
			tp = tp->log_newer_thread;
			counter++;
			if(found_in && found_out){
				break;
			}
		}

		// Removes a removed thread from the removed threads list if we find the timestamp indicating this remove
		// means we did a complete loop on the buffer so we have no more data of this thread
		// Only possible if the buffer have already been completely filled, otherwise the 0s would trigger the condition
		if(_full){
			if(((_threads_log[_fill_pos] & THREAD_OUT_MASK) >> THREAD_OUT_POS) == ((_threads_log[_fill_pos] & THREAD_IN_MASK) >> THREAD_IN_POS)){

				_next_thread_removed_to_delete = (_next_thread_removed_to_delete+1) % MAX_REMOVED_THREADS;
				_threads_removed_count--;
			}
		}

		// Creates a record if at least one of the threads need to be logged 
		if(in->log_this_thread || out->log_this_thread){
			_threads_log[_fill_pos] = ((time << TIME_POS) & TIME_MASK) 
										| ((counter_out << THREAD_OUT_POS) & THREAD_OUT_MASK)
										| ((counter_in << THREAD_IN_POS) & THREAD_IN_MASK);
			_increments_fill_pos();
		}
	}
#else
	(void) ntp;
	(void) otp;
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

/********************                PUBLIC FUNCTIONS              ********************/

void printUcUsage(BaseSequentialStream* out) {
	
	thread_t *tp;

	static uint64_t sum;
	static uint16_t tmp1, tmp2;
	sum = 0;
	//takes the first thread created
	tp = chRegFirstThread();
	//computes the total number of cycles counted
	do {
		sum += tp->stats.cumulative;
		tp = chRegNextThread(tp);
	} while (tp != NULL);
	sum += ch.kernel_stats.m_crit_thd.cumulative;
	sum += ch.kernel_stats.m_crit_isr.cumulative;

	//takes the first thread created
	tp = chRegFirstThread();
	//computes the percentage of time used by each thread
	do {
		tmp1 = (uint16_t)(tp->stats.cumulative*10000/sum);
		chprintf(out, "%25s          %u.%u%%\r\n", 	tp->name == NULL ? "NONAME" : tp->name, 
													tmp1/100, tmp1%100);
		tp = chRegNextThread(tp);
	} while (tp != NULL);

	tmp1 = (uint16_t)(ch.kernel_stats.m_crit_thd.cumulative*10000/sum);
	tmp2 = (uint16_t)(ch.kernel_stats.m_crit_isr.cumulative*10000/sum);

	chprintf(out, "\r\n            critical thd:%u.%u%%   critical isr:%u.%u%%\r\n",
	  tmp1/100, tmp1%100,tmp2/100, tmp2%100);
	chprintf(out, "\r\n");
}

void printStatThreads(BaseSequentialStream *out)
{
	static const char *states[] = {CH_STATE_NAMES};
	thread_t *tp;
	size_t n = 0;
	size_t sz;
	uint32_t used_pct;

	chprintf(out, "\r\n");
	chprintf(out, "     begin        end   size   used    %% prio     state         name\r\n");
	chprintf(out, "--------------------------------------------------------------------\r\n");

	tp = chRegFirstThread();
	//special print for the main thread because apparently its memory is handled differently
	chprintf(out, "0x%08lx 0x%08lx    ???    ???  ??? %4lu %9s %12s\r\n", (uint32_t)tp->wabase,
																		 (uint32_t)tp,
																		 (uint32_t)tp->prio,
																		 states[tp->state],
																		 tp->name == NULL ? "NONAME" : tp->name);

	tp = chRegNextThread(tp);
	do {
		n = 0;

		uint8_t *begin = (uint8_t *)tp->wabase;
		uint8_t *end = (uint8_t *)tp;
		sz = end - begin;

		while(begin < end)
		if(*begin++ != CH_DBG_STACK_FILL_VALUE) ++n;

		used_pct = (n * 100) / sz;

		chprintf(out, "0x%08lx 0x%08lx %6u %6u %3u%% %4lu %9s %12s\r\n", (uint32_t)tp->wabase,
																		 (uint32_t)tp,
																		 sz,
																		 n,
																		 used_pct,
																		 (uint32_t)tp->prio,
																		 states[tp->state],
																		 tp->name == NULL ? "NONAME" : tp->name);

		tp = chRegNextThread(tp);
	} while (tp != NULL);

	chprintf(out, "\r\n");
}

void printListThreads(BaseSequentialStream *out){

	thread_t *tp;

	uint16_t n = 1;

	tp = first_added_thread;
	while(tp != NULL){
		chprintf(out, "Thread number %2d : Prio = %3d, Log = %3s, Name = %s\r\n",	
				n,
				tp->prio,
#ifdef ENABLE_THREADS_TIMESTAMPS
				tp->log_this_thread ? "Yes" : "No",
#else
				"No",
#endif /* ENABLE_THREADS_TIMESTAMPS */
				tp->name == NULL ? "NONAME" : tp->name); 
		tp = tp->log_newer_thread;
		n++;
	}

#ifdef ENABLE_THREADS_TIMESTAMPS
	chprintf(out,"Deleted threads: \r\n");

	n = _next_thread_removed_to_delete;
	for(uint8_t i = 0 ; i < _threads_removed_count ; i++){

		// Case if we have a deleted dynamic thread
		if( (((uint32_t)_threads_removed_infos[n]) & DYN_THD_REMOVED_MASK) == DYN_THD_REMOVED_MASK ){
			chprintf(out, "Thread number %2d : Prio = %3d, Log = %3s, Name = %s\r\n",	
					_threads_removed[n],
					((uint32_t)_threads_removed_infos[n] & DYN_THD_PRIO_MASK) >> DYN_THD_PRIO_POS,
					(((uint32_t)_threads_removed_infos[n] & DYN_THD_LOG_MASK) >> DYN_THD_LOG_POS) ? "Yes" : "No",
					generic_dynamic_thread_name); 

		// Case if we have a static deleted thread
		}else{
			chprintf(out, "Thread number %2d : Prio = %3d, Log = %3s, Name = %s\r\n",	
					_threads_removed[n],
					_threads_removed_infos[n]->prio,
					_threads_removed_infos[n]->log_this_thread ? "Yes" : "No",
					_threads_removed_infos[n]->name == NULL ? "NONAME" : _threads_removed_infos[n]->name); 
		}

		n = (n+1) % MAX_REMOVED_THREADS;
	}
#endif /* ENABLE_THREADS_TIMESTAMPS */
}
const char* setTriggerTimestamps(const char* trigger_name){
#ifdef ENABLE_THREADS_TIMESTAMPS
	static const char* name = NULL;
	if(!_triggered){
		chSysLock();
		name = trigger_name;
		_triggered = true;
		_trigger_time = chVTGetSystemTimeX();
		if(!_full && _fill_pos <= THREADS_TIMESTAMPS_LOG_SIZE/2){
			// Special case when we trigger before the logs are completely populated and we
			// have less that half of the buffer full
			// In this case we fill completely the remaining buffer and not only half of it
			_fill_remaining = THREADS_TIMESTAMPS_LOG_SIZE - _fill_pos;
		}else{
			_fill_remaining = THREADS_TIMESTAMPS_LOG_SIZE/2;
		}
		chSysUnlock();
	}
	return name;
#endif /* ENABLE_THREADS_TIMESTAMPS */
	return NULL;
}

void resetTriggerTimestamps(void){
#ifdef ENABLE_THREADS_TIMESTAMPS
	if(_triggered){
		chSysLock();
		_triggered = false;
		_trigger_time = 0;
		_fill_remaining = 0;
		// Also resets the logs because otherwise we pollute with old values from the trigger
		// It's a problem because we can miss OUT or IN times, which destroy the timeline
		// and if kept, there is also a big blank between the old and new values since the recording of
		// the timestamps was paused
		_fill_pos = 0;
		_full = false;
		// Also resets the history of deleted threads since we reset the logs
		_threads_removed_pos = 0;
		_next_thread_removed_to_delete = 0;
		_threads_removed_count = 0;

		chSysUnlock();
	}
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

uint8_t getLogSetting(void){
#ifdef ENABLE_THREADS_TIMESTAMPS
	return logSetting;
#else
	return false;
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

void logThisThreadTimestamps(void){
#ifdef ENABLE_THREADS_TIMESTAMPS
	thread_t* tp = chThdGetSelfX();
	tp->log_this_thread = true;
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

void dontLogThisThreadTimestamps(void){
#ifdef ENABLE_THREADS_TIMESTAMPS
	thread_t* tp = chThdGetSelfX();
	tp->log_this_thread = false;
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

void logNextCreatedThreadsTimestamps(void){
#ifdef ENABLE_THREADS_TIMESTAMPS
	logSetting = true;
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

void dontLogNextCreatedThreadsTimestamps(void){
#ifdef ENABLE_THREADS_TIMESTAMPS
	logSetting = false;
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

void printTimestampsThread(BaseSequentialStream *out){
#ifdef ENABLE_THREADS_TIMESTAMPS
	static uint8_t thread_in = 0;
	static uint8_t thread_out = 0;
	static uint32_t time = 0;
	static uint32_t pos = 0;
	static uint32_t limit = 0;

	// temporarily pauses the filling of the logs
	_pause = true;

	if(_triggered){
		chprintf(out, "Triggered at %7d\r\n", _trigger_time);
	}

	// Special case if the logs aren't full of data
	if(!_full){
		limit = _fill_pos;
		pos = 0;
	}else{
		pos = _fill_pos;
		limit = THREADS_TIMESTAMPS_LOG_SIZE;
	}
	
	for(uint32_t i = 0 ; i < limit ; i++){
		thread_in = (_threads_log[pos] & THREAD_IN_MASK) >> THREAD_IN_POS;
		thread_out = (_threads_log[pos] & THREAD_OUT_MASK) >> THREAD_OUT_POS;
		time = (_threads_log[pos] & TIME_MASK) >> TIME_POS;

		chprintf(out, "From %2d to %2d at %7d\r\n", thread_out, thread_in, time);

		pos = (pos+1) % THREADS_TIMESTAMPS_LOG_SIZE;
	}

	_pause = false;
#else
	chprintf(chp, "%s", no_timestamps_error_message);
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

/********************                SHELL FUNCTIONS               ********************/

void cmd_threads_list(BaseSequentialStream *chp, int argc, char *argv[])
{   
    (void)argc;
    (void)argv;

    if (argc > 0) {
        chprintf(chp, "Usage: threads_list\r\n");
        return;
    }
    
    printListThreads(chp);

}

void cmd_threads_timestamps_trigger(BaseSequentialStream *chp, int argc, char *argv[])
{   
    (void)argc;
    (void)argv;

    if (argc > 0) {
        chprintf(chp, "Usage: threads_timestamps_trigger\r\n");
        return;
    }
#ifdef ENABLE_THREADS_TIMESTAMPS
    const char* name = setTriggerTimestamps("Shell command");
    chprintf(chp, "Trigger set by %s at %7d\r\n",name, _trigger_time);
#else
	chprintf(chp, "%s", no_timestamps_error_message);
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

void cmd_threads_timestamps_run(BaseSequentialStream *chp, int argc, char *argv[])
{   
    (void)argc;
    (void)argv;

    if (argc > 0) {
        chprintf(chp, "Usage: threads_timestamps_run\r\n");
        return;
    }
#ifdef ENABLE_THREADS_TIMESTAMPS
    resetTriggerTimestamps();
    chprintf(chp, "Run mode\r\n");
#else
	chprintf(chp, "%s", no_timestamps_error_message);
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

void cmd_threads_timestamps(BaseSequentialStream *chp, int argc, char *argv[])
{   
    (void)argc;
    (void)argv;

    if (argc > 0) {
        chprintf(chp, "Usage: threads_timestamps\r\n");
        return;
    }
#ifdef ENABLE_THREADS_TIMESTAMPS
    printTimestampsThread(chp);
#else
	chprintf(chp, "%s", no_timestamps_error_message);
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

void cmd_threads_stat(BaseSequentialStream *chp, int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printStatThreads(chp);
}

void cmd_threads_uc(BaseSequentialStream *chp, int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printUcUsage(chp);
}