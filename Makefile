CC = gcc
BIN = bin
OBJ = obj
SRC = src
TARGET = $(BIN)/test
SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

CC_COMMON = -std=c11 -march=native -D_DEFAULT_SOURCE
CC_DEBUG = -g -Wall -Wextra -DDEBUG -fsanitize=undefined,address
CC_RELEASE = -O2
LD_COMMON = 
LD_DEBUG = -fsanitize=undefined,address
LD_RELEASE = 

CCFLAGS = $(CC_COMMON) $(CC_DEBUG)
LDFLAGS = $(LD_COMMON) $(LD_DEBUG)
release: CCFLAGS = $(CC_COMMON) $(CC_RELEASE)
release: LDFLAGS = $(LD_COMMON) $(LD_RELEASE)

debug: $(TARGET)
-include $(DEPS)
release: clean $(TARGET)

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) -MMD $(CCFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(TARGET) $(DEPS) $(OBJS)
