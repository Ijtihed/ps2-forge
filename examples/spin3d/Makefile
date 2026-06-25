EE_BIN  = spin3d.elf
EE_OBJS = game.o engine.o
EE_INCS  += -I$(GSKIT)/include -I$(PS2SDK)/ee/include -I../../engine
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS = -lgskit_toolkit -lgskit -ldmakit -laudsrv -lpad -lm -lc
vpath %.c ../../engine
SECS ?= 3
all: $(EE_BIN)
run: $(EE_BIN)
	Play --elf $(EE_BIN)
shot: $(EE_BIN)
	bash ../../tools/shot.sh $(EE_BIN) $(SECS) shot.png
clean:
	rm -f $(EE_BIN) $(EE_OBJS)
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
