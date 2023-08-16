
SRC = bibakBOXServer.c bibakBOXClient.c src/mycommon.c src/thread_pool.c src/task_queue.c src/message_impl.c src/file_ops.c
HDR = include/mycommon.h include/thread_pool.h include/task_queue.h include/message_impl.h include/file_ops.h
FLAGS = -Wall -Wextra -std=gnu99
LFLAGS = -lrt -pthread
TRGT1 = BibakBOXServer
TRGT2 = BibakBOXClient
VFLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes

OBJS1 = obj/bibakBOXServer.o \
	   obj/mycommon.o \
	   obj/thread_pool.o \
	   obj/task_queue.o \
	   obj/file_ops.o \
	   obj/message_impl.o

OBJS2 = obj/bibakBOXClient.o \
	   obj/mycommon.o \
	   obj/thread_pool.o \
	   obj/task_queue.o \
	   obj/file_ops.o \
	   obj/message_impl.o

all: $(TRGT1) $(TRGT2)

obj/%.o: src/%.c $(HDR)
	mkdir -p obj
	gcc $(FLAGS) -c -o $@ $<

obj/%.o: %.c $(HDR)
	mkdir -p obj
	gcc $(FLAGS) -c -o $@ $<

$(TRGT1): $(OBJS1)
	gcc $(FLAGS) -o $(TRGT1) $(OBJS1) $(LFLAGS)

$(TRGT2): $(OBJS2)
	gcc $(FLAGS) -o $(TRGT2) $(OBJS2) $(LFLAGS)

clean:
	rm -rf obj $(TRGT1) $(TRGT2)

valgrind: $(TRGT1)
	valgrind $(VFLAGS) ./$(TRGT1) server_dir 10 5001

valgrind2: $(TRGT2)
	valgrind $(VFLAGS) ./$(TRGT2) client_dir 5001
