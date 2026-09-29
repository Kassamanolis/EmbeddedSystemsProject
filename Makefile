CC = gcc
 
CFLAGS = -Wall -Wextra -O2 -Iinclude $(shell pkg-config --cflags libcjson)
 
LDFLAGS = -lpthread -lm $(shell pkg-config --libs libwebsockets libcjson)
 
SRC_DIR = .
 
SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/producer.c $(SRC_DIR)/consumer.c \
          $(SRC_DIR)/monitor.c $(SRC_DIR)/circular_buffer.c $(SRC_DIR)/counters.c
 
BIN = rt_firehose
RUN_SECONDS = 86400  #24 hours * 60 minutes * 60 seconds
 
.PHONY: all run run4ev clean
 
all:
	$(CC) $(CFLAGS) $(SOURCES) -o $(BIN) $(LDFLAGS)
 
run: all
	timeout $(RUN_SECONDS) ./$(BIN)
 
run4ev: all
	./$(BIN)
 
clean:
	rm -f $(BIN) metrics_log.txt



