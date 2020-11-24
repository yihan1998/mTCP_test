APP = linux_test

SRCS = main.c

CC = gcc
CFLAGS = -g -O3
LDFLAGS = -lpthread 

INC = -I ./include

all: $(APP)

$(APP): $(SRCS)
	$(CC) $(CFLAGS) $^ $(INC) -o $@ $(LDFLAGS) 

clean:
	rm -f *.o $(APP)