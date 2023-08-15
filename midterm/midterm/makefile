
SRC1 = src/mycommon.c biboServer.c src/bserver.c
SRC2 = src/mycommon.c biboClient.c
SRC3 = src/mycommon.c src/child_server.c
HDR1 = include/mycommon.h include/bserver.h
HDR2 = include/mycommon.h
HDR3 = include/mycommon.h
FLAGS = -Wall -Wextra -std=gnu99
LFLAGS = -lrt -pthread
TRGT1 = biboServer
TRGT2 = biboClient
TRGT3 = src/child_server
VFLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes
LOGDIR = sv_log

all: $(TRGT1) $(TRGT2) $(TRGT3) logd

$(TRGT1): $(SRC1) $(HDR1)
	gcc $(FLAGS) $(SRC1) $(LFLAGS) -o $(TRGT1)

$(TRGT2): $(SRC2) $(HDR2)
	gcc $(FLAGS) $(SRC2) $(LFLAGS) -o $(TRGT2)

$(TRGT3): $(SRC3) $(HDR3)
	gcc $(FLAGS) $(SRC3) $(LFLAGS) -o $(TRGT3)

clean:
	rm -f $(TRGT1) $(TRGT2) $(TRGT3)

valgrind1: $(TRGT1)
	valgrind $(VFLAGS) ./$(TRGT1)

valgrind2: $(TRGT2)
	valgrind $(VFLAGS) ./$(TRGT2)

debug1: $(TRGT1)
	gdb ./$(TRGT1)

debug2: $(TRGT2)
	gdb ./$(TRGT2)

logd:
	if [ ! -d $(LOGDIR) ]; then mkdir $(LOGDIR); fi
