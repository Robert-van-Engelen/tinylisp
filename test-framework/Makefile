CC=gcc
CFLAGS=-std=c2x -g -Wall -DUNITY_INCLUDE_CONFIG_H
#CFLAGS.test-tl-commented += -DDEBUG

#LINK_COMMON=lex.o
TEST_OBJ_COMMON=../test-framework/unity.o test-util.o $(LINK_COMMON)
EXE_OBJ_COMMON=main.o $(LINK_COMMON)

.DEFAULT_GOAL := all

.PHONY: all
all: unity.o 

# tinylisp-float tinylisp-float-opt
.PHONY: clean
clean: 
	find . -type f -perm /u+x -exec rm  {} \;
	rm -f *.o
