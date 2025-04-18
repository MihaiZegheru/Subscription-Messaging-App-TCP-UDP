SERVER_BIN=server
SUBSCRIBER_BIN=subscriber

SRCPATHS=src

SERVER_SRC=$(SRCPATHS)/server.cpp
SUBSCRIBER_SRC=$(SRCPATHS)/subscriber.cpp


CFLAGS=-Wall -Werror -Wno-error=unused-variable
CC=g++

server: $(SERVER_BIN)

subscriber: $(SUBSCRIBER_BIN)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ 

$(SUBSCRIBER_BIN): $(SUBSCRIBER_SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(SERVER_BIN) $(SUBSCRIBER_BIN) 
