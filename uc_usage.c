/**
 * @file	uc_usage.c
 * @brief  	Functions to show the uC usage
 * 
 * @source			http://www.chibios.com/forum/viewtopic.php?f=2&t=138&start=10
 * @modified by  	Eliot Ferragni
 */
 
#include "ch.h"
#include "chprintf.h"
#include "uc_usage.h"

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
		chprintf(out, "%12s %u.%u%%\r\n", tp->p_name, tmp1/100, tmp1%100);
		tp = chRegNextThread(tp);
	} while (tp != NULL);

	tmp1 = (uint16_t)(ch.kernel_stats.m_crit_thd.cumulative*10000/sum);
	tmp2 = (uint16_t)(ch.kernel_stats.m_crit_isr.cumulative*10000/sum);

	chprintf(out, "critical thd:%u.%u%%   critical isr:%u.%u%%\r\n",
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
			                                                             tp->p_name == NULL ? "" : tp->p_name);

	tp = chRegNextThread(tp);
	do {
		n = 0;

		uint8_t *begin = (uint8_t *)tp;
		uint8_t *end = (uint8_t *)tp->p_ctx.r13;
		sz = end - begin - sizeof(tp->log);	//removes the log tab as well

		while(begin < end)
		if(*begin++ != CH_DBG_STACK_FILL_VALUE) ++n;

		n -= sizeof(tp->log); //removes the log tab as well

		used_pct = (n * 100) / sz;

		chprintf(out, "0x%08lx 0x%08lx %6u %6u %3u%% %4lu %9s %12s\r\n", (uint32_t)tp,                                                                                           
																		 (uint32_t)tp->p_ctx.r13,
			                                                             sz,
			                                                             n,
			                                                             used_pct,
			                                                             (uint32_t)tp->p_prio,
			                                                             states[tp->p_state],
			                                                             tp->p_name == NULL ? "" : tp->p_name);

		tp = chRegNextThread(tp);
	} while (tp != NULL);

	chprintf(out, "\r\n");
}