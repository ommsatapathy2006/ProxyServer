CC = gcc
CFLAGS = -Wall -Wextra -pthread -Iinclude -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
SRCS = src/main.c src/server.c src/http_handler.c src/cache.c src/access_control.c src/logger.c src/error_handler.c
OBJS = $(SRCS:.c=.o)
TARGET = proxy

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: tests/test_cache tests/test_http_handler
	./tests/test_cache
	./tests/test_http_handler

tests/test_cache: tests/test_cache.c src/cache.c
	$(CC) $(CFLAGS) -o $@ tests/test_cache.c src/cache.c

tests/test_http_handler: tests/test_http_handler.c src/http_handler.c src/cache.c src/access_control.c src/logger.c src/error_handler.c
	$(CC) $(CFLAGS) -o $@ tests/test_http_handler.c src/http_handler.c src/cache.c src/access_control.c src/logger.c src/error_handler.c

clean:
	rm -f $(OBJS) $(TARGET) tests/test_cache tests/test_http_handler logs/*.log

.PHONY: all test clean
