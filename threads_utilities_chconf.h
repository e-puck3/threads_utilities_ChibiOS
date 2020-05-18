

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