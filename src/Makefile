CC=clang
CFLAGS=-std=c2x -g -Wall
#CFLAGS.test-tl-commented += -DDEBUG

#LINK_COMMON=lex.o
TEST_OBJ_COMMON=../test-framework/unity.o $(LINK_COMMON)
EXE_OBJ_COMMON=main.o $(LINK_COMMON)

.DEFAULT_GOAL := all

.PHONY: all
all: tinylisp-commented tinylisp-readline tinylisp tinylisp-opt  \
	 test-tl-commented 

# tinylisp-float tinylisp-float-opt
.PHONY: clean
clean: 
	find . -type f -perm /u+x -exec rm  {} \;
	rm -f *.o

tinylisp-commented: tinylisp-commented.o $(EXE_OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@  $^

tinylisp-readline: tinylisp-readline.o
	$(CC) $(CFLAGS) -o $@  $^ -lreadline

test-tl-commented: test-tl-commented.o tinylisp-commented.o $(TEST_OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@  $^


