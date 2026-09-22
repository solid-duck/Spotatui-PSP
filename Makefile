TARGET = SpotatuiPSP
OBJS = main.o

INCDIR =
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

LIBDIR =
LIBS = -lpspgu -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpspnet -lpspnet_apctl

BUILD_PRX = 1
PSP_FW_VERSION = 600

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Spotatui PSP
PSP_EBOOT_ICON = ICON0.PNG

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak