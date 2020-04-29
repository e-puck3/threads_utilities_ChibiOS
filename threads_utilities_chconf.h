
/**
 * Declaration of fillThreadsTimestamps()
 * To be included with TIMESTAMPS_INCLUDE defined
 * before CH_CFG_CONTEXT_SWITCH_HOOK() in chconf.h
 */

#ifdef TIMESTAMPS_INCLUDE

#define THREAD_NB(x) (1 << x)

#if !defined(_FROM_ASM_)
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
void fillThreadsTimestamps(void* in, void* out);
void removeThread(void* otp);
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _FROM_ASM_ */

#undef TIMESTAMPS_INCLUDE
#endif /* TIMESTAMPS_INCLUDE */

