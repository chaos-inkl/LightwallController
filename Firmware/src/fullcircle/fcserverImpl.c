#include "fcserverImpl.h"
#include <stdio.h>
#include <string.h>
#include "chprintf.h"
#include "ch.h"

#include <unistd.h>
#include "fcserver.h"
#include "customHwal.h"	/* Needed to activate debugging in server implementation */

#include "dmx/dmx.h"

#define MAILBOX_SIZE		10
#define MAILBOX2_SIZE		5

#define FCS_PRINT( ... )	if (gDebugShell) { chprintf(gDebugShell, __VA_ARGS__); }

/* Mailbox, filled by the fc_server thread */
static uint32_t buffer4mailbox[MAILBOX_SIZE];
static MAILBOX_DECL(mailboxOut, buffer4mailbox, MAILBOX_SIZE);

/* Mailbox, checked by the fc_server thread */
static uint32_t buffer4mailbox2[MAILBOX2_SIZE];
static MAILBOX_DECL(mailboxIn, buffer4mailbox2, MAILBOX2_SIZE);

static BaseSequentialStream * gDebugShell = NULL;

static uint32_t gServerActive = 0;

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

void handleInputMailbox(void)
{
	msg_t msg1, msg2, status;
	int newMessages;
	
	/* Use nonblocking function to count incoming messages */
	newMessages = chMBGetUsedCountI(&mailboxIn);
	
	if (newMessages >= 2)
	{
		/* First retrieve the given pointer */
		status = chMBFetch(&mailboxIn, &msg1, TIME_INFINITE);
		if (status == RDY_OK)
		{
			status = chMBFetch(&mailboxIn, &msg2, TIME_INFINITE);
			if (status == RDY_OK)
			{
				chSysLock();
				switch ((uint32_t) msg1) {
					case 1:
						gDebugShell = (BaseSequentialStream *) msg2;
						chprintf((BaseSequentialStream *) msg2, "Debugging works\r\n");
						hwal_init((BaseSequentialStream *) msg2);
						break;
					case 2:
						FCS_PRINT("FC Server - silent mode\r\n");
						gDebugShell = 0;
						break;
					case 3:
						gServerActive = (uint32_t) msg2;
						FCS_PRINT("DynFc Server - DMX is set to %d\r\n", gServerActive);
						break;
					default:
						break;
				}
				
				chSysUnlock();
			}
		}
	}
}

/******************************************************************************
 * IMPLEMENTATION FOR THE NECESSARY CALLBACKS
 ******************************************************************************/

void onNewImage(uint8_t* rgb24Buffer, int width, int height)
{
	FCS_PRINT("%d x %d\r\n", width, height);
	
	/*FIXME there is no MAPPING between positions and DMX addresses */
	
	/* Set the DMX buffer */
	dmx_buffer.length = width * height * 3;
	memcpy(dmx_buffer.buffer, rgb24Buffer, dmx_buffer.length);
}

void onClientChange(uint8_t totalAmount, fclientstatus_t action, int clientsocket)
{
	if (gDebugShell) {
		chprintf(gDebugShell, "Callback client %d did %X '", clientsocket, action);
		switch (action) {
			case FCCLIENT_STATUS_WAITING:
				chprintf(gDebugShell, "waiting for a GO");
				break;
			case FCCLIENT_STATUS_CONNECTED:
				chprintf(gDebugShell, "is CONNECTED to the wall");
				break;
			case FCCLIENT_STATUS_DISCONNECTED:
				chprintf(gDebugShell, "has left");	
				break;
			case FCCLIENT_STATUS_INITING:
				chprintf(gDebugShell, "found this server");	
				break;
			case FCCLIENT_STATUS_TOOMUTCH:
				chprintf(gDebugShell, "is one too mutch");	
				break;
			default:
				break;
		}
		chprintf(gDebugShell, "'\t[%d clients]\r\n", totalAmount);
	}
	
	chSysLock();
	chMBPostI(&mailboxOut, (uint32_t) action);
	chMBPostI(&mailboxOut, (uint32_t) clientsocket);
	chSysUnlock();
}

/******************************************************************************
 * GLOBAL FUNCTIONS
 ******************************************************************************/

/**
 * Stack area for the dynamic fullcircle thread.
 */
WORKING_AREA(wa_fc_server, FCSERVERIMPL_THREAD_STACK_SIZE);

/**
 * Dynamic fullcircle server thread.
 */
msg_t fc_server(void *p)
{		
	fcserver_ret_t	ret;
	fcserver_t		server;
	
	chRegSetThreadName("fcdynserver");
	(void)p;
	
	/* Prepare Mailbox to communicate with the others */
	chMBInit(&mailboxOut, (msg_t *)buffer4mailbox, MAILBOX_SIZE);
	chMBInit(&mailboxIn, (msg_t *)buffer4mailbox2, MAILBOX2_SIZE);
	
	ret = fcserver_init(&server, &onNewImage, &onClientChange, 
						10 /* width of wall */, 12 /* height of wall */);
	
	if (ret != FCSERVER_RET_OK)
	{
		/* printf("Server initialization failed with returncode %d\n", ret); */
		return FR_INT_ERR;
	}
		
	fcserver_setactive(&server, 1 /* TRUE */);
	
	do {
		handleInputMailbox();
		
		ret = fcserver_process(&server);
		
		chThdSleep(MS2ST(FCSERVER_IMPL_SLEEPTIME /* convert milliseconds to system ticks */));
	} while ( ret == FCSERVER_RET_OK);
	
	FCS_PRINT("FATAL error, closing thread\r\n");
	
	/* clean everything */
	fcserver_close(&server);
	
	return RDY_OK;
}

FRESULT fcsserverImpl_cmdline(BaseSequentialStream *chp, int argc, char *argv[])
{
	FRESULT res = FR_OK;
	msg_t msg1, msg2, status;
	int i, newMessages;
	
	if(argc < 1)
	{
		chprintf(chp, "Usage {status, debugOn, debugOff, on, off}\r\n");
		res = FR_INT_ERR;
		return res;
	}
	else if(argc >= 1)
    {
		if (strcmp(argv[0], "status") == 0)
		{
			newMessages = chMBGetUsedCountI(&mailboxOut);
			
			chprintf(chp, "%d Messages found\r\n", newMessages );
			for (i=0; i < newMessages; i += 2) {
				status = chMBFetch(&mailboxOut, &msg1, TIME_INFINITE);
				
				if (status != RDY_OK)
				{
					chprintf(chp, "Failed accessing message queue: %d\r\n", status );
				}
				else
				{
					status = chMBFetch(&mailboxOut, &msg2, TIME_INFINITE);
					if (status == RDY_OK)
					{
						chSysLock();
						chprintf(chp, "%d = %d\r\n", (uint32_t) msg1, (uint32_t) msg2);
						chSysUnlock();
					}
					else
					{
						chprintf(chp, "Could only extract key (%d)\r\n", (uint32_t) msg1);
					}

				}
			}
		}
		else if (strcmp(argv[0], "debugOn") == 0)
		{
			/* Activate the debugging */
			chprintf(chp, "Activate the logging for fullcircle server\r\n");
			chSysLock();
			chMBPostI(&mailboxIn, (uint32_t) 1);
			chMBPostI(&mailboxIn, (uint32_t) chp);
			chSysUnlock();
		}
		else if (strcmp(argv[0], "debugOff") == 0)
		{
			/* Activate the debugging */
			chprintf(chp, "Deactivate the logging for fullcircle server\r\n");
			chSysLock();
			chMBPostI(&mailboxIn, (uint32_t) 2);
			chMBPostI(&mailboxIn, (uint32_t) 0);
			chSysUnlock();
		}
		else if (strcmp(argv[0], "on") == 0)
		{
			chprintf(chp, "Activate DMX output\r\n");
			chSysLock();
			chMBPostI(&mailboxIn, (uint32_t) 3);
			chMBPostI(&mailboxIn, (uint32_t) 1);
			chSysUnlock();
		}
		else if (strcmp(argv[0], "off") == 0)
		{
			chprintf(chp, "Turn DMX output OFF\r\n");
			chSysLock();
			chMBPostI(&mailboxIn, (uint32_t) 3);
			chMBPostI(&mailboxIn, (uint32_t) 0);
			chSysUnlock();
		}
	}
	
	return res;
}