/**
 * @file	threads_utilities.c
 * @brief  	Functions to obtain various stats about the threads
 * 
 * @source			http://www.chibios.com/forum/viewtopic.php?f=2&t=138&start=10
 * @source2			http://www.chibios.com/forum/viewtopic.php?t=4496
 * @created by  	Eliot Ferragni
 * @last modif		9 april 2020
 */

#include <stdlib.h>
#include "threads_utilities.h"

/********************              INTERNAL VARIABLES              ********************/

// we can store the in and out time of 63 threads max
// each case of the tabs corresponds to 1 system tick
#ifdef ENABLE_THREADS_TIMESTAMPS

// define by the mask we apply for the timestamps. We have 6 bits for the thread number
// and the thread 0 reserved 
#define MAX_HANDLED_THREADS	64
#define HANDLED_THREADS_LEN (MAX_HANDLED_THREADS - 1)

static uint8_t logSetting = THREADS_TIMESTAMPS_DEFAULT_LOG;

static uint32_t _threads_log[THREADS_TIMESTAMPS_LOG_SIZE] = {0};

static thread_t* _threads_removed_infos[HANDLED_THREADS_LEN] = {NULL};
static uint8_t _threads_removed[HANDLED_THREADS_LEN] = {0};
static uint8_t _threads_removed_pos = 0;
static uint8_t _next_thread_removed_to_delete = 0;
static uint8_t _threads_removed_count = 0;

static uint32_t _fill_pos = 0;
static uint8_t _pause = false;
static uint8_t _full = false;

static uint8_t _triggered = false;
static uint32_t _trigger_time = 0;
static int32_t _fill_remaining = 0;

#endif /* ENABLE_THREADS_TIMESTAMPS */

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

/********************               PRIVATE FUNCTIONS              ********************/


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

/********************                CHCONF FUNCTION               ********************/

/**
 * @brief 			Saves the exiting thread into the removed thread list to keep trace of the 
 * 					deleted threads for the timestamps functionality.
 * 					To be called by the CH_CFG_THREAD_EXIT_HOOK in chconf.h
 * 
 * @param device 	Pointer to the exiting thread
 */	
void removeThread(void* otp){

	static thread_t *tp = 0;
	static thread_t *out = NULL;
	static uint8_t counter_out = 0;
	static uint32_t time = 0;

	time = chVTGetSystemTimeX();

	tp = ch.rlist.r_newer;
	out = (thread_t*) otp;
	counter_out = 1;
	while(tp != (thread_t *)&ch.rlist){
		if(tp == out){
			break;
		}
		tp = tp->p_newer;
		counter_out++;
	}

	if(_threads_removed_count < HANDLED_THREADS_LEN){
		_threads_log[_fill_pos] = ((time << TIME_POS) & TIME_MASK) 
								| ((counter_out << THREAD_OUT_POS) & THREAD_OUT_MASK)
								| ((counter_out << THREAD_IN_POS) & THREAD_IN_MASK);
		_increments_fill_pos();

		_threads_removed[_threads_removed_pos] = counter_out;
		_threads_removed_infos[_threads_removed_pos] = out; 

		_threads_removed_pos = (_threads_removed_pos+1) % HANDLED_THREADS_LEN;
		_threads_removed_count++;
	}

}

/**
 * @brief 			Saves the IN and OUT times of each thread to log while the buffers aren't full.
 * 					To be called by the CH_CFG_CONTEXT_SWITCH_HOOK in chconf.h
 * 
 * @param device 	Pointer to the output
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

	if(_continue_to_fill()){
		in = (thread_t*) ntp;
		out = (thread_t*) otp;
		tp = ch.rlist.r_newer;
		counter = 1;
		counter_in = 0;
		counter_out = 0;
		found_in = false;
		found_out = false;
		while(tp != (thread_t *)&ch.rlist){
			if(tp == in){
				found_in = true;
				counter_in = counter;
			}
			if(tp == out){
				found_out = true;
				counter_out = counter;
			}
			tp = tp->p_newer;
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

				_next_thread_removed_to_delete = (_next_thread_removed_to_delete+1) % HANDLED_THREADS_LEN;
				_threads_removed_count--;
			}
		}

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
		sum += tp->p_stats.cumulative;
		tp = chRegNextThread(tp);
	} while (tp != NULL);
	sum += ch.kernel_stats.m_crit_thd.cumulative;
	sum += ch.kernel_stats.m_crit_isr.cumulative;

	//takes the first thread created
	tp = chRegFirstThread();
	//computes the percentage of time used by each thread
	do {
		tmp1 = (uint16_t)(tp->p_stats.cumulative*10000/sum);
		chprintf(out, "%25s          %u.%u%%\r\n", 	tp->p_name == NULL ? "NONAME" : tp->p_name, 
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
	chprintf(out, "0x%08lx 0x%08lx    ???    ???  ??? %4lu %9s %12s\r\n", (uint32_t)tp,
																		 (uint32_t)tp->p_ctx.r13,
																		 (uint32_t)tp->p_prio,
																		 states[tp->p_state],
																		 tp->p_name == NULL ? "NONAME" : tp->p_name);

	tp = chRegNextThread(tp);
	do {
		n = 0;

		uint8_t *begin = (uint8_t *)tp;
		uint8_t *end = (uint8_t *)tp->p_ctx.r13;
		sz = end - begin;

		while(begin < end)
		if(*begin++ != CH_DBG_STACK_FILL_VALUE) ++n;

		used_pct = (n * 100) / sz;

		chprintf(out, "0x%08lx 0x%08lx %6u %6u %3u%% %4lu %9s %12s\r\n", (uint32_t)tp,
																		 (uint32_t)tp->p_ctx.r13,
																		 sz,
																		 n,
																		 used_pct,
																		 (uint32_t)tp->p_prio,
																		 states[tp->p_state],
																		 tp->p_name == NULL ? "NONAME" : tp->p_name);

		tp = chRegNextThread(tp);
	} while (tp != NULL);

	chprintf(out, "\r\n");
}

void printListThreads(BaseSequentialStream *out){

	thread_t *tp;

	uint16_t n = 1;

	tp = chRegFirstThread();
	while(tp != NULL){
		chprintf(out, "Thread number %2d : Prio = %3d, Log = %3s, Name = %s\r\n",	
				n,
				tp->p_prio,
#ifdef ENABLE_THREADS_TIMESTAMPS
				tp->log_this_thread ? "Yes" : "No",
#else
				"No",
#endif /* ENABLE_THREADS_TIMESTAMPS */
				tp->p_name == NULL ? "NONAME" : tp->p_name); 
		tp = chRegNextThread(tp);
		n++;
	}

	chprintf(out,"Deleted threads: \r\n");

	n = _next_thread_removed_to_delete;
	for(uint8_t i = 0 ; i < _threads_removed_count ; i++){
		chprintf(out, "Thread number %2d : Prio = %3d, Log = %3s, Name = %s\r\n",	
				_threads_removed[n],
				_threads_removed_infos[n]->p_prio,
#ifdef ENABLE_THREADS_TIMESTAMPS
				_threads_removed_infos[n]->log_this_thread ? "Yes" : "No",
#else
				"No",
#endif /* ENABLE_THREADS_TIMESTAMPS */
				_threads_removed_infos[n]->p_name == NULL ? "NONAME" : _threads_removed_infos[n]->p_name); 
		n = (n+1) % HANDLED_THREADS_LEN;
	}
}

void setTriggerTimestamps(void){
#ifdef ENABLE_THREADS_TIMESTAMPS
	if(!_triggered){
		chSysLock();
		_triggered = true;
		_trigger_time = chVTGetSystemTimeX();
		if(!_full && _fill_pos <= THREADS_TIMESTAMPS_LOG_SIZE/2){
			// Special case when we trigger before the logs are completely populated and we
			// have less that half of hte buffer full
			// In this case we fill completely the remaining buffer and not only half of it
			_fill_remaining = THREADS_TIMESTAMPS_LOG_SIZE - _fill_pos;
		}else{
			_fill_remaining = THREADS_TIMESTAMPS_LOG_SIZE/2;
		}
		chSysUnlock();
	}
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

void resetTriggerTimestamps(void){
#ifdef ENABLE_THREADS_TIMESTAMPS
	if(_triggered){
		chSysLock();
		_triggered = false;
		_trigger_time = 0;
		_fill_remaining = 0;
		// Also resets the logs because otherwise we polute with old values from the trigger
		// It's a problem because we can miss OUT or IN times, which destroy the timeline
		// and if kept, ther is also a big blank between the old and new values since the recording of
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
	return logSetting;
}

void logThisThreadTimestamps(void){
	thread_t* tp = chThdGetSelfX();
	tp->log_this_thread = true;
}

void dontLogThisThreadTimestamps(void){
	thread_t* tp = chThdGetSelfX();
	tp->log_this_thread = false;
}

void logNextCreatedThreadsTimestamps(void){
	logSetting = true;
}

void dontLogNextCreatedThreadsTimestamps(void){
	logSetting = false;
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
	chprintf(out, "The thread timestamps functionality is disabled\r\n");
	chprintf(out, "Please define USE_THREADS_TIMESTAMPS = true in your makefile \r\n");
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

    setTriggerTimestamps();
    chprintf(chp, "Trigger set at %7d\r\n", _trigger_time);
}

void cmd_threads_timestamps_run(BaseSequentialStream *chp, int argc, char *argv[])
{   
    (void)argc;
    (void)argv;

    if (argc > 0) {
        chprintf(chp, "Usage: threads_timestamps_run\r\n");
        return;
    }

    resetTriggerTimestamps();
    chprintf(chp, "Run mode\r\n");
}

void cmd_threads_timestamps(BaseSequentialStream *chp, int argc, char *argv[])
{   
    (void)argc;
    (void)argv;

    if (argc > 0) {
        chprintf(chp, "Usage: threads_timestamps\r\n");
        return;
    }

    printTimestampsThread(chp);
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