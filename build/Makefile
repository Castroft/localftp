# Makefile for Unix/Linux
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -std=c99 -D_POSIX_C_SOURCE=200809L
TARGET = lftp

# 指定源文件路径
SDK_ROOT = $(LFTP_DIR)

INCLUDES += -I$(SDK_ROOT)/include

include $(SDK_ROOT)/cli/Makefile
include $(SDK_ROOT)/common/Makefile
include $(SDK_ROOT)/core/Makefile


CFLAGS += $(INCLUDES)
LD_FLAGS += -fno-common -g -lpthread -ldl -lm -lrt

SRC_FILES := $(shell echo $(SRC_FILES)|sed 's/ /\n/g'|sort|uniq|tr -t '\n' '')
SDK_OBJS = $(SRC_FILES:.c=.o)

.PHONY: all
all: $(TARGET)

# 链接
$(TARGET): $(SDK_OBJS)
	$(CC) $^ -o $@ ${LD_FLAGS}


.PHONY: clean
clean:
	rm -f $(SDK_OBJS)