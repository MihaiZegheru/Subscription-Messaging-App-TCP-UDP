SERVER_BIN=server
SUBSCRIBER_BIN=subscriber

LOGPATH=log
SERVER_LOG=$(LOGPATH)/server.log
CLIENT_LOG=$(LOGPATH)/client.log

SRCPATH=src
INCLUDEPATH=include

SRCS=$(wildcard $(SRCPATH)/*.cpp)

CXXFLAGS=-Wall -Werror -Wno-error=unused-variable -I$(INCLUDEPATH) -DDEBUG
CXX=g++

SERVER_SRC=$(SRCPATH)/server.cpp
SUBSCRIBER_SRC=$(SRCPATH)/subscriber.cpp

all: $(SERVER_BIN) $(SUBSCRIBER_BIN)

debug: CXXFLAGS += -DDEBUG
debug: clean all

$(SERVER_BIN): $(SERVER_SRC) $(filter-out $(SUBSCRIBER_SRC),$(SRCS))
	$(CXX) $(CXXFLAGS) -DLOG_FILE=\"$(SERVER_LOG)\" -o $@ $^

$(SUBSCRIBER_BIN): $(SUBSCRIBER_SRC) $(filter-out $(SERVER_SRC),$(SRCS))
	$(CXX) $(CXXFLAGS) -DLOG_FILE=\"$(CLIENT_LOG)\" -o $@ $^

clean:
	rm -rf $(SERVER_BIN) $(SUBSCRIBER_BIN)
