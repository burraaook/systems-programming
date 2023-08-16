
SRC = main.c src/mycommon.c src/thread_pool.c src/task_queue.c
HDR = include/mycommon.h include/thread_pool.h include/task_queue.h 
FLAGS = -Wall -Wextra -std=gnu99
LFLAGS = -lrt -pthread
TRGT = main
VFLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes

OBJS = obj/main.o \
	   obj/mycommon.o \
	   obj/thread_pool.o \
	   obj/task_queue.o

all: $(TRGT)

obj/%.o: src/%.c $(HDR)
	mkdir -p obj
	gcc $(FLAGS) -c -o $@ $<

obj/%.o: %.c $(HDR)
	mkdir -p obj
	gcc $(FLAGS) -c -o $@ $<

$(TRGT): $(OBJS)
	gcc $(FLAGS) -o $(TRGT) $(OBJS) $(LFLAGS)

clean:
	rm -rf obj $(TRGT)

valgrind:
	valgrind $(VFLAGS) ./$(TRGT) 30 15 source dest

