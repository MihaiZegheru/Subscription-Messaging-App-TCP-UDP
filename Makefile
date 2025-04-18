SERVER_BIN=server
SUBSCRIBER_BIN=subscriber

SRCPATHS=src

SERVER_SRC=$(SRCPATHS)/server.cpp
SUBSCRIBER_SRC=$(SRCPATHS)/subscriber.cpp


CXXFLAGS=-Wall -Werror -Wno-error=unused-variable
CXX=g++

all: $(SERVER_BIN) $(SUBSCRIBER_BIN)

debug: CXXFLAGS += -DDEBUG
debug: clean all

$(SERVER_BIN): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ 

$(SUBSCRIBER_BIN): $(SUBSCRIBER_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf $(SERVER_BIN) $(SUBSCRIBER_BIN) 
