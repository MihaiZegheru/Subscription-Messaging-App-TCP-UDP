SERVER_BIN=server
SUBSCRIBER_BIN=subscriber

SRCPATHS=src
INCLUDEPATH=include

SRCS=$(wildcard $(SRCPATHS)/*.cpp)

CXXFLAGS=-Wall -Werror -Wno-error=unused-variable -I$(INCLUDEPATH) -DDEBUG
CXX=g++

SERVER_SRC=$(SRCPATHS)/server.cpp
SUBSCRIBER_SRC=$(SRCPATHS)/subscriber.cpp

all: $(SERVER_BIN) $(SUBSCRIBER_BIN)

debug: CXXFLAGS += -DDEBUG
debug: clean all

$(SERVER_BIN): $(SERVER_SRC) $(filter-out $(SUBSCRIBER_SRC),$(SRCS))
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SUBSCRIBER_BIN): $(SUBSCRIBER_SRC) $(filter-out $(SERVER_SRC),$(SRCS))
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf $(SERVER_BIN) $(SUBSCRIBER_BIN)
