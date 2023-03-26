SRC := aesdsocket.c
TARGET = aesdsocket
OBJS := $(SRC:.c=.o)
CC = gcc
CFLAGS = -g -Wall -Werror -pthread

all: $(TARGET)

$(TARGET) : $(TARGET).c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	-rm -f *.o $(TARGET) *.elf *.map
