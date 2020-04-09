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
static uint32_t threads_log_in[THREADS_TIMESTAMPS_LOG_SIZE] = {0};
static uint32_t threads_log_out[THREADS_TIMESTAMPS_LOG_SIZE] = {0};

#define CASE_INIT 			0
#define CASE_IN 			1
#define CASE_OUT 			2

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

void printCountThreads(BaseSequentialStream *out){

	thread_t *tp;

	uint16_t n = 1;

	tp = chRegFirstThread();
	tp = chRegNextThread(tp);
	while(tp != NULL){
		n++;
		tp = chRegNextThread(tp);
	}
	
	chprintf(out, "Number of threads : %d\r\n",n); 
}

void fillThreadsTimestamps(void* ntp, void* otp){
#ifdef ENABLE_THREADS_TIMESTAMPS
	static uint32_t time = 0;
	static thread_t *tp = 0;
	static thread_t *in = NULL;
	static thread_t *out = NULL;
	static uint8_t counter = 0;
	time = chVTGetSystemTimeX();
	if(time < THREADS_TIMESTAMPS_LOG_SIZE){
		in = (thread_t*) ntp;
		out = (thread_t*) otp;
		tp = ch.rlist.r_newer;
		counter = 0;
		while(tp != (thread_t *)&ch.rlist){
			if(tp == in){
				threads_log_in[time] |= (1 << counter);
				break;
			}
			tp = tp->p_newer;
			counter++;
		}
		tp = ch.rlist.r_newer;
		counter = 0;
		while(tp != (thread_t *)&ch.rlist){
			if(tp == out){
				threads_log_out[time] |= (1 << counter);
				break;
			}
			tp = tp->p_newer;
			counter++;
		}
	}
#else
	(void) ntp;
	(void) otp;
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

void printTimestampsThread(BaseSequentialStream *out, uint8_t thread_number){
#ifdef ENABLE_THREADS_TIMESTAMPS
	static uint8_t last_case = CASE_INIT;
	static thread_t *tp = NULL;
	static uint8_t n = 0;
	static uint16_t time_in = 0;
	static uint16_t time_out = 0;

	n = thread_number;
	tp = chRegFirstThread();
	while(n){
		tp = chRegNextThread(tp);
		n--;
		if(tp == NULL){
			chprintf(out, "This thread doesn't exist\r\n");
			return;
		}
	}
   
	chprintf(out, "Thread : %s\r\n",tp->p_name == NULL ? "NONAME" : tp->p_name); 
	chprintf(out, "Prio : %d\r\n",tp->p_prio); 

	last_case = CASE_INIT;

	for(uint16_t i = 0; i < THREADS_TIMESTAMPS_LOG_SIZE; i++){
		time_in = (threads_log_in[i] & (1 << thread_number));
		time_out = (threads_log_out[i] & (1 << thread_number));


		switch(last_case){
			case CASE_INIT:
				if((time_in && time_out) || (!time_in && time_out)){
					chprintf(out, "%d\r\n", i);	//prints in time
					chprintf(out, "%d\r\n", i); //prints out time
					last_case = CASE_OUT;
				}
				else if(time_in && !time_out){
					chprintf(out, "%d\r\n", i);	//prints in time
					last_case = CASE_IN;
				}
				break;
			case CASE_IN:
				if(time_out && !time_in){
					chprintf(out, "%d\r\n", i);	//prints out time
					last_case = CASE_OUT;
				}
				else if(time_in && time_out){
					chprintf(out, "%d\r\n", i);	//prints out time
					chprintf(out, "%d\r\n", i);	//prints in time
					last_case = CASE_IN;
				}
				break;
			case CASE_OUT:
				if(time_in && !time_out){
					chprintf(out, "%d\r\n", i);	//prints in time
					last_case = CASE_IN;
				}
				else if(time_in && time_out){
					chprintf(out, "%d\r\n", i);	//prints in time
					chprintf(out, "%d\r\n", i);	//prints out time
					last_case = CASE_OUT;
				}
				break;
		}
		
	}
#else
	chprintf(out, "The thread timestamps functionnality isn't enabled\r\n");
	chprintf(out, "Please define ENABLE_THREADS_TIMESTAMPS in your chconf.h file \r\n");
#endif /* ENABLE_THREADS_TIMESTAMPS */
}

/********************                SHELL FUNCTIONS               ********************/

void cmd_threads_count(BaseSequentialStream *chp, int argc, char *argv[])
{   
    (void)argc;
    (void)argv;

    if (argc > 0) {
        chprintf(chp, "Usage: threads_count\r\n");
        return;
    }
    
    printCountThreads(chp);

}

void cmd_threads_timeline(BaseSequentialStream *chp, int argc, char *argv[])
{   
    (void)argc;
    (void)argv;

    if (argc != 1) {
        chprintf(chp, "Usage: threads_timeline numberOfTheThread\r\n");
        return;
    }
    
    uint8_t n = atoi(argv[0]);

    printTimestampsThread(chp, n);
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