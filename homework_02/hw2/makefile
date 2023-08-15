
SRC = myshell.c src/mycommon.c src/shell_impl.c
HDR = include/mycommon.h include/shell_impl.h
FLAGS = -Wall -Wextra -Werror
TRGT = myshell
VFLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes
LOGDIR = log

all: $(TRGT)

$(TRGT): $(SRC) $(HDR)
	gcc $(FLAGS) $(SRC) -o $(TRGT)

logd:
	if [ ! -d $(LOGDIR) ]; then mkdir $(LOGDIR); fi

clean:
	rm -f $(TRGT) $(LOGDIR)/*

run: $(TRGT)
	./$(TRGT)

valgrind: $(TRGT)
	valgrind $(VFLAGS) ./$(TRGT)

debug: $(TRGT)
	gdb ./$(TRGT)
