SRC ?= aesdsocket.c
TARGET ?= aesdsocket
OBJS := $(SRC:.c=.o)
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -lpthread -lrt

all: $(TARGET)

$(TARGET) : $(TARGET).c
	$(CC) $(CFLAGS) -o $(INCLUDES) $(TARGET) $(SRC) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map
