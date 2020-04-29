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

// we can store the in and out time of 32 threads max (1 bit per thread)
// each case of the tabs corresponds to 1 system tick
#ifdef ENABLE_THREADS_TIMESTAMPS

static uint32_t _threads_log[THREADS_TIMESTAMPS_LOG_SIZE] = {0};
static uint64_t _threads_log_mask = TIMESTAMPS_THREADS_TO_LOG;

static thread_t* threads_removed_infos[64] = {NULL};
static uint8_t threads_removed[64] = {0};

static uint32_t _fill_pos = 0;
static uint8_t _pause = false;
static uint8_t _full = false;

static uint8_t _triggered = false;
static uint32_t _trigger_time = 0;
static int32_t _fill_remaining = 0;

#endif /* ENABLE_THREADS_TIMESTAMPS */

// max 63 Threads (1-63), 0 means nothing
// bit 31 to bit 12 = time
// bit 11 to bit 6 	= out thread number (1-63)	
// bit 5  to bit 0 	= in thread number (1-63)
#define THREAD_IN_POS		0
#define THREAD_IN_MASK		(0x3F << THREAD_IN_POS)
#define THREAD_OUT_POS		6
#define THREAD_OUT_MASK		(0x3F << THREAD_OUT_POS)
#define TIME_POS			12
#define TIME_MASK			(0xFFFFF << TIME_POS)

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
				(_threads_log_mask & (1 << n)) ? "Yes" : "No",
#else
				"No",
#endif /* ENABLE_THREADS_TIMESTAMPS */
				tp->p_name == NULL ? "NONAME" : tp->p_name); 
		tp = chRegNextThread(tp);
		n++;
	}

	chprintf(out,"Deleted threads: \r\n");
	n = 1;
	while(n < 64){
		if(threads_removed[n]){
			chprintf(out, "Thread number %2d : Prio = %3d, Log = %3s, Name = %s\r\n",	
					threads_removed[n],
					threads_removed_infos[n]->p_prio,
#ifdef ENABLE_THREADS_TIMESTAMPS
					(_threads_log_mask & (1 << n)) ? "Yes" : "No",
#else
					"No",
#endif /* ENABLE_THREADS_TIMESTAMPS */
					threads_removed_infos[n]->p_name == NULL ? "NONAME" : threads_removed_infos[n]->p_name); 
		}
		n++;
	}
}

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

void setTriggerTimestamps(void){
#ifdef ENABLE_THREADS_TIMESTAMPS
	if(!_triggered){
		chSysLock();
		_triggered = true;
		_trigger_time = chVTGetSystemTimeX();
		if(_full){
			_fill_remaining = THREADS_TIMESTAMPS_LOG_SIZE/2;
		}else{
			// Special case when we trigger before the logs are completely populated
			// In this case we fill completely the remaining buffer and not only half of it
			_fill_remaining = THREADS_TIMESTAMPS_LOG_SIZE - _fill_pos;
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
		_fill_pos = 0;
		_full = false;
		chSysUnlock();
	}
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

void removeThread(void* otp){

	static thread_t *tp = 0;
	static thread_t *out = NULL;
	static uint8_t counter_out = 0;
	static uint8_t counter_removed = 0;
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
	_threads_log[_fill_pos] = ((time << TIME_POS) & TIME_MASK) 
							| ((counter_out << THREAD_OUT_POS) & THREAD_OUT_MASK)
							| ((counter_out << THREAD_IN_POS) & THREAD_IN_MASK);
	_increments_fill_pos();

	counter_removed = 1;
	while(threads_removed[counter_removed] != 0){
		counter_removed++;
	}

	threads_removed[counter_removed] = counter_out;
	threads_removed_infos[counter_removed] = out; 
}

void fillThreadsTimestamps(void* ntp, void* otp){
#ifdef ENABLE_THREADS_TIMESTAMPS
	static uint32_t time = 0;
	static thread_t *tp = 0;
	static thread_t *in = NULL;
	static thread_t *out = NULL;
	static uint8_t counter_in = 0;
	static uint8_t counter_out = 0;
	static uint8_t found_in = false;
	static uint8_t found_out = false;
	static uint8_t deleted_thd = 0;

	time = chVTGetSystemTimeX();

	if(_continue_to_fill()){
		in = (thread_t*) ntp;
		out = (thread_t*) otp;
		tp = ch.rlist.r_newer;
		counter_in = 1;
		found_in = false;
		while(tp != (thread_t *)&ch.rlist){
			if(tp == in){
				found_in = true;
				break;
			}
			tp = tp->p_newer;
			counter_in++;
		}
		if(!found_in){
			counter_in = 0;
		}

		tp = ch.rlist.r_newer;
		counter_out = 1;
		found_out = false;
		while(tp != (thread_t *)&ch.rlist){
			if(tp == out){
				found_out = true;
				break;
			}
			tp = tp->p_newer;
			counter_out++;
		}
		if(!found_out){
			counter_out = 0;
		}

		deleted_thd = (_threads_log[_fill_pos] & THREAD_OUT_MASK) >> THREAD_OUT_POS;
		if(deleted_thd == ((_threads_log[_fill_pos] & THREAD_IN_MASK) >> THREAD_IN_POS)){
			uint8_t counter_removed = 0;
			while(threads_removed[counter_removed] != deleted_thd){
				counter_removed++;
			}
			threads_removed[counter_removed] = 0;
			threads_removed_infos[counter_removed] = NULL; 
		}

		if((_threads_log_mask & (1 << counter_in)) || (_threads_log_mask & (1 << counter_out))){
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

		if((_threads_log_mask & (1 << thread_in)) || (_threads_log_mask & (1 << thread_out))){
			time = (_threads_log[pos] & TIME_MASK) >> TIME_POS;
			chprintf(out, "From %2d to %2d at %7d\r\n", thread_out, thread_in, time);
		}

		pos = (pos+1) % THREADS_TIMESTAMPS_LOG_SIZE;
	}

	_pause = false;
#else
	chprintf(out, "The thread timestamps functionnality is disabled\r\n");
	chprintf(out, "Please define USE_THREADS_TIMESTAMPS = yes in your makefile \r\n");
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