# ps2-forge game Makefile. Set EE_BIN to your game name.
EE_BIN  = game.elf
EE_OBJS = game.o engine.o

EE_INCS  += -I$(GSKIT)/include -I$(PS2SDK)/ee/include -I../../engine
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS = -lgskit_toolkit -lgskit -ldmakit -laudsrv -lpad -lm -lc

vpath %.c ../../engine        # find engine.c

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
