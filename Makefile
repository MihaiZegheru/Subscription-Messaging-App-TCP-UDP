SERVER_BIN=server
SUBSCRIBER_BIN=subscriber

SRCPATHS=src

SERVER_SRC=$(SRCPATHS)/server.c
SUBSCRIBER_SRC=$(SRCPATHS)/subscriber.c


CFLAGS=-Wall -Werror -Wno-error=unused-variable
CC=gcc

server: $(SERVER_BIN)

subscriber: $(SUBSCRIBER_BIN)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ 

$(SUBSCRIBER_BIN): $(SUBSCRIBER_SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(SERVER_BIN) $(SUBSCRIBER_BIN) 
