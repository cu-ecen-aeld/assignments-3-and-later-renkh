SRC := writer.c
TARGET = writer
OBJS := $(SRC:.c=.o)
CC = gcc
CFLAGS = -g -Wall

all: $(TARGET)

$(TARGET) : $(TARGET).c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	-rm -f *.o $(TARGET) *.elf *.map
