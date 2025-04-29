SERVER_BIN=server
SUBSCRIBER_BIN=subscriber

LOG_PATH=log
SERVER_LOG=$(LOG_PATH)/server.log
CLIENT_LOG=$(LOG_PATH)/client.log

SRC_PATH=src
INCLUDE_PATH=include

SRCS=$(wildcard $(SRC_PATH)/*.cpp)

CXXFLAGS=-std=c++17 -Wall -Werror -Wno-error=unused-variable -I$(INCLUDE_PATH)
CXX=g++

SERVER_SRC=$(SRC_PATH)/server.cpp
SUBSCRIBER_SRC=$(SRC_PATH)/subscriber.cpp

all: $(SERVER_BIN) $(SUBSCRIBER_BIN)

debug: CXXFLAGS += -DDEBUG
debug: clean all

$(SERVER_BIN): $(SERVER_SRC) $(filter-out $(SUBSCRIBER_SRC),$(SRCS))
	$(CXX) $(CXXFLAGS) -DLOG_FILE=\"$(SERVER_LOG)\" -o $@ $^

$(SUBSCRIBER_BIN): $(SUBSCRIBER_SRC) $(filter-out $(SERVER_SRC),$(SRCS))
	$(CXX) $(CXXFLAGS) -DLOG_FILE=\"$(CLIENT_LOG)\" -o $@ $^

clean:
	rm -rf $(SERVER_BIN) $(SUBSCRIBER_BIN)
