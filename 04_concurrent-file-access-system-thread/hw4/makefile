
SRC1 = src/mycommon.c biboServer.c src/bserver.c src/task_queue.c src/thread_pool.c src/command_impl.c
SRC2 = src/mycommon.c biboClient.c
HDR1 = include/mycommon.h include/bserver.h include/task_queue.h include/thread_pool.h include/command_impl.h
HDR2 = include/mycommon.h include/biboClient.h
FLAGS = -Wall -Wextra -std=gnu99
LFLAGS = -lrt -pthread
TRGT1 = biboServer
TRGT2 = biboClient
TRGT3 = src/child_server
VFLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes
LOGDIR = sv_log

all: $(TRGT1) $(TRGT2) logd

$(TRGT1): $(SRC1) $(HDR1)
	gcc $(FLAGS) $(SRC1) $(LFLAGS) -o $(TRGT1)

$(TRGT2): $(SRC2) $(HDR2)
	gcc $(FLAGS) $(SRC2) $(LFLAGS) -o $(TRGT2)

clean:
	rm -f $(TRGT1) $(TRGT2) $(TRGT3)

logd:
	if [ ! -d $(LOGDIR) ]; then mkdir $(LOGDIR); fi
