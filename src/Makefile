CC=gcc
CFLAGS=-std=c2x -g -Wall
#CFLAGS.test-tl-commented += -DDEBUG

#LINK_COMMON=lex.o
TEST_OBJ_COMMON=../test-framework/unity.o $(LINK_COMMON)
EXE_OBJ_COMMON=main.o $(LINK_COMMON)

.DEFAULT_GOAL := all

.PHONY: all
all: tinylisp-commented tinylisp-readline tinylisp-tested tinylisp tinylisp-opt  \
	 test-tl-tested 

# tinylisp-float tinylisp-float-opt
.PHONY: clean
clean: 
	find . -type f -perm /u+x -exec rm  {} \;
	rm -f *.o

tinylisp-commented: tinylisp-commented.o $(EXE_OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@  $^

tinylisp-readline: tinylisp-readline.o
	$(CC) $(CFLAGS) -o $@  $^ -lreadline

tinylisp-tested: tinylisp-tested.o $(EXE_OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@  $^

test-tl-tested: test-tl-tested.o tinylisp-tested.o $(TEST_OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@  $^


