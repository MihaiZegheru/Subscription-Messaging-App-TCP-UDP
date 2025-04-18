SERVER_BIN=server
SUBSCRIBER_BIN=subscriber

SRCPATHS=src

SERVER_SRC=$(SRCPATHS)/server.cpp
SUBSCRIBER_SRC=$(SRCPATHS)/subscriber.cpp


CFLAGS=-Wall -Werror -Wno-error=unused-variable
CXX=g++

$(SERVER_BIN): $(SERVER_SRC)
	$(CXX) $(CFLAGS) -o $@ $^ 

$(SUBSCRIBER_BIN): $(SUBSCRIBER_SRC)
	$(CXX) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(SERVER_BIN) $(SUBSCRIBER_BIN) 
