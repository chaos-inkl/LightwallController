/* @file fcserverImpl.h
 * @brief Dynamic fullcircle implementation for Chibios
 * @author Ollo
 *
 * @date 26.09.2013
 * @defgroup LightwallController
 *
 */

#include "ch.h"
#include "hal.h"
#include "ff.h"


#ifndef FCSERVER_IMPL_H
#define FCSERVER_IMPL_H

#ifndef FCSERVERIMPL_THREAD_STACK_SIZE
#define FCSERVERIMPL_THREAD_STACK_SIZE   THD_WA_SIZE(4096)
#endif

#ifndef FCSERVER_IMPL_SLEEPTIME
#define FCSERVER_IMPL_SLEEPTIME 20	/* time in milliseconds before starting endless loop again */
#endif

extern WORKING_AREA(wa_fc_server, FCSERVERIMPL_THREAD_STACK_SIZE);

#ifdef __cplusplus
extern "C" {
#endif
	msg_t fc_server(void *p);
#ifdef __cplusplus
}
#endif


/**
 * Debug interface for the commandline
 */
FRESULT fcsserverImpl_cmdline(BaseSequentialStream *chp, int argc, char *argv[]);

#endif /* End of FCSERVER_IMPL_H */