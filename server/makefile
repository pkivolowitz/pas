CC	= g++
CFLAGS	= -g -Wall -std=c++11 -fopenmp
LDFLAGS	= -lpthread -lsqlite3 -lavcodec -lavformat -fopenmp
SRC		= $(wildcard *.cpp)
OBJS	= $(SRC:.cpp=.o)

server.out				: server.o network_component.o file_system_component.o
						$(CC) $^ -o $@ $(LDFLAGS)

%.o						: %.cpp
						$(CC) -c $(CFLAGS) $< -o $@

server.o				: server.cpp network_component.hpp file_system_component.hpp

network_component.o		: network_component.cpp network_component.hpp

file_system_component.o	: file_system_component.cpp file_system_component.hpp

clean					:
						rm -f server $(OBJS)