/** @file fcserverImpl.h
 * @brief Dynamic fullcircle implementation for Chibios
 * @author Ollo
 *
 * @date 26.09.2013
 * @defgroup LightwallController
 *
 */

#include "fcserverImpl.h"
#include <stdio.h>
#include <string.h>
#include "chprintf.h"
#include "ch.h"

#include <unistd.h>
#include "fcserver.h"
#include "fcscheduler.h"
#include "customHwal.h"	/* Needed to activate debugging in server implementation */

#include "dmx/dmx.h"

#define OUTPUT_MAILBOX_SIZE		10
#define FCSERVER_MAILBOX_SIZE		5

#define FCS_PRINT( ... )	if (gDebugShell) { chprintf(gDebugShell, __VA_ARGS__); }

/******************************************************************************
 * GLOBAL VARIABLES of this module
 ******************************************************************************/

uint32_t gFcServerActive = 0;

/******************************************************************************
 * LOCAL VARIABLES for this module
 ******************************************************************************/

/* Mailbox, checked by the fc_server thread */
uint32_t*	gFcServerMailboxBuffer	= NULL;
Mailbox *	gFcServerMailbox 		= NULL;

static BaseSequentialStream * gDebugShell = NULL;

static wallconf_t wallcfg;

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

static int
handleInputMailbox(void)
{
  msg_t msg1, msg2, status;
  int newMessages;
  int mailboxCmd;

  /* Use nonblocking function to count incoming messages */
  newMessages = chMBGetUsedCountI(gFcServerMailbox);

  if (newMessages >= 2)
    {
      /* First retrieve the given pointer */
      status = chMBFetch(gFcServerMailbox, &msg1, TIME_INFINITE);
      if (status == RDY_OK)
        {
          status = chMBFetch(gFcServerMailbox, &msg2, TIME_INFINITE);
          if (status == RDY_OK)
            {
              chSysLock();
              switch ((uint32_t) msg1)
                {
              case FCSERVER_CMD_DEBUG_ON:
                gDebugShell = (BaseSequentialStream *) msg2;
                chprintf((BaseSequentialStream *) msg2,
                    "FC Server - Debugging active\r\n");
                hwal_init((BaseSequentialStream *) msg2);
                break;
              case FCSERVER_CMD_DEBUG_OFF:
                FCS_PRINT("FC Server - silent mode\r\n");
                gDebugShell = 0;
                break;
              case FCSERVER_CMD_MODIFY_ACTIVE:
                gFcServerActive = (uint32_t) msg2;
                FCS_PRINT("DynFc Server - DMX is set to %d\r\n",
                    gFcServerActive);
                break;
              case FCSERVER_CMD_MODIFY_DISCONNECTALL:
            	  mailboxCmd = FCSERVER_CMD_MODIFY_DISCONNECTALL;
            	  break;
              default:
                break;
                }

              chSysUnlock();
            }
        }
    }
  	return mailboxCmd;
}

/******************************************************************************
 * IMPLEMENTATION FOR THE NECESSARY CALLBACKS
 ******************************************************************************/

void
onNewImage(uint8_t* rgb24Buffer, int width, int height)
{
  if (gFcServerActive)
    {
      /* Write the DMX buffer */
      fcsched_printFrame(rgb24Buffer, width, height, &wallcfg);
      FCS_PRINT("Update Frame\r\n");
      chSysLock();
      chMBPostI(gFcMailboxDyn, (uint32_t) 1);
      chSysUnlock();
    }
}

void
onClientChange(uint8_t totalAmount, fclientstatus_t action, int clientsocket)
{
  /* Update the scheduler about the actual amount of connected client. */
  gFcConnectedClients = totalAmount;

  if (gDebugShell)
    {
      chprintf(gDebugShell, "FcServer - Callback client %d did %X '",
          clientsocket, action);
      switch (action)
        {
      case FCCLIENT_STATUS_WAITING:
        chprintf(gDebugShell, "waiting for a GO");
        break;
      case FCCLIENT_STATUS_CONNECTED:
        chprintf(gDebugShell, "is CONNECTED to the wall");
        break;
      case FCCLIENT_STATUS_DISCONNECTED:
        chprintf(gDebugShell, "has left");
        /* The actual client left -> update the counter */
        if (gFcConnectedClients > 0)
          {
            gFcConnectedClients--;
          }
        break;
      case FCCLIENT_STATUS_INITING:
        chprintf(gDebugShell, "found this server");
        break;
      case FCCLIENT_STATUS_TOOMUTCH:
        chprintf(gDebugShell, "is one too much");
        break;
      default:
        break;
        }
      chprintf(gDebugShell, "'\t[%d clients]\r\n", totalAmount);
    }

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
msg_t
fc_server(void *p)
{
  fcserver_ret_t ret;
  fcserver_t server;
  chRegSetThreadName("fcdynserver");
  (void) p;
  int mailboxCmd;

  /* small hack to initialize the global accessible server */
  gFcServerMailboxBuffer = hwal_malloc(sizeof(uint32_t) * FCSERVER_MAILBOX_SIZE);
  MAILBOX_DECL(fcServerMailbox, gFcServerMailboxBuffer, FCSERVER_MAILBOX_SIZE);
  gFcServerMailbox = &fcServerMailbox;


  /* Prepare Mailbox to communicate with the others */
  chMBInit(gFcServerMailbox, (msg_t *) gFcServerMailboxBuffer, FCSERVER_MAILBOX_SIZE);

  /* read the dimension from the configuration file */
  readConfigurationFile(&wallcfg);

  ret = fcserver_init(&server, &onNewImage, &onClientChange, wallcfg.width,
      wallcfg.height);

  if (ret != FCSERVER_RET_OK)
    {
      /* printf("Server initialization failed with returncode %d\n", ret); */
      return FR_INT_ERR;
    }

  do
    {
	  mailboxCmd = handleInputMailbox();
      switch(mailboxCmd) {
      case FCSERVER_CMD_MODIFY_DISCONNECTALL:
    	  fcserver_disconnect_all(&server);
    	  break;
      default:
    	  break;
      }

      ret = fcserver_process(&server, FCSERVER_IMPL_SLEEPTIME);

      fcserver_setactive(&server, gFcServerActive);

      chThdSleep(
          MS2ST(
              FCSERVER_IMPL_SLEEPTIME /* convert milliseconds to system ticks */));
    }
  while (ret == FCSERVER_RET_OK);

  FCS_PRINT("FATAL error, closing fullcircle server thread\r\n");

  /* clean the memory of the configuration */
  if (wallcfg.pLookupTable)
    {
      hwal_free(wallcfg.pLookupTable);
    }

  /* clean everything */
  fcserver_close(&server);

  return RDY_OK;
}

void
fcsserverImpl_cmdline(BaseSequentialStream *chp, int argc, char *argv[])
{
  if (argc < 1)
    {
      chprintf(chp, "Usage {debugOn, debugOff, on, off, disconnect}\r\n");
      return;
    }
  else if (argc >= 1)
    {
      if (strcmp(argv[0], "debugOn") == 0)
        {
          /* Activate the debugging */
          chprintf(chp, "Activate the logging for fullcircle server\r\n");
          chSysLock()
          ;
          chMBPostI(gFcServerMailbox, (uint32_t) FCSERVER_CMD_DEBUG_ON);
          chMBPostI(gFcServerMailbox, (uint32_t) chp);
          chSysUnlock();
        }
      else if (strcmp(argv[0], "debugOff") == 0)
        {
          /* Activate the debugging */
          chprintf(chp, "Deactivate the logging for fullcircle server\r\n");
          chSysLock();
          chMBPostI(gFcServerMailbox, (uint32_t) FCSERVER_CMD_DEBUG_OFF);
          chMBPostI(gFcServerMailbox, (uint32_t) 0);
          chSysUnlock();
        }
      else if (strcmp(argv[0], "on") == 0)
        {
          chprintf(chp, "Activate DMX output\r\n");
          chSysLock();
          chMBPostI(gFcServerMailbox, (uint32_t) FCSERVER_CMD_MODIFY_ACTIVE);
          chMBPostI(gFcServerMailbox, (uint32_t) 1);
          chSysUnlock();
        }
      else if (strcmp(argv[0], "off") == 0)
        {
          chprintf(chp, "Turn DMX output OFF\r\n");
          chSysLock();
          chMBPostI(gFcServerMailbox, (uint32_t) FCSERVER_CMD_MODIFY_ACTIVE);
          chMBPostI(gFcServerMailbox, (uint32_t) 0);
          chSysUnlock();
        }
      else if (strcmp(argv[0], "disconnect") == 0)
      {
        chprintf(chp, "Disconnect all clients\r\n");
        chSysLock();
        chMBPostI(gFcServerMailbox, (uint32_t) FCSERVER_CMD_MODIFY_DISCONNECTALL);
        chMBPostI(gFcServerMailbox, (uint32_t) 1);
        chSysUnlock();
      }
    }
}
