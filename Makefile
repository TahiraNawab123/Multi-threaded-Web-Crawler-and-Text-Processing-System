CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g -pthread
LDFLAGS = -pthread -lcurl

TARGET = crawler
SRCS   = main.c queue.c downloader.c parser.c aggregator.c utils.c metrics.c
OBJS   = $(SRCS:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(TARGET)
	./$(TARGET) URLs.txt 6 200

clean:
	rm -f $(OBJS) $(TARGET) metrics.csv
