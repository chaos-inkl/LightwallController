# List of all the board related files.
APPSRC = src/main.c \
       src/dmx/dmx.c \
       src/dmx/dmx_cmd.c \
       src/dmx/rgb.c \
       src/cmd/cmd_threads.c \
       src/cmd/cmd_mem.c \
<<<<<<< HEAD
       src/cmd/cmd_cat.c \
       src/cmd/cmd_flash.c \
       src/ini/ini.c \
       src/conf/conf.c \
       src/fullcircle/fcserverImpl.c \
       src/fullcircle/fcscheduler.c 
=======
       
>>>>>>> e96f50159decb18da3e939dc5bb729621fcf4474

# Required include directories
APPINC = ${APP} \
       src/fullcircle

# List all user C define here
APPDEFS = -DSHELL_MAX_ARGUMENTS=6

# Fullcricle (fc_c) specific:
# Debugging for the underling library
<<<<<<< HEAD
#APPDEFS += -DPRINT_DEBUG


# Append the WALL
APPSRC += src/ugfx/fcwall.c \
		  src/ugfx/ugfx_util.c \
		  src/ugfx/ugfx_cmd.c
		  		  
APPINC += src/ugfx
APPDEFS += -DUGFX_WALL
=======
# APPDEFS += -DPRINT_DEBUG

#APPDEFS += -DWITH_TELNET
APPDEFS += -DFILESYSTEM_ONLY

>>>>>>> e96f50159decb18da3e939dc5bb729621fcf4474
