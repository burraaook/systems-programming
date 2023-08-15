
SRC1 = appendMeMore.c src/mycommon.c
SRC2 = src/customdups.c src/mycommon.c dupsTest.c
HDR1 = include/mycommon.h
HDR2 = include/customdups.h include/mycommon.h
FLAGS = -Wall -Wextra -Werror
TRGT1 = appendMeMore
TRGT2 = dupsTest

all: $(TRGT1) $(TRGT2)

$(TRGT1): $(SRC1) $(HDR1)
	gcc $(SRC1) $(FLAGS) -o $(TRGT1)

$(TRGT2): $(SRC2) $(HDR2)
	gcc $(SRC2) $(FLAGS) -o $(TRGT2)

clean:
	rm -f $(TRGT1) $(TRGT2)

run: $(TRGT2)
	./$(TRGT2)

