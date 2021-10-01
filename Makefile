RM = rm -f
CXX = g++
CXXFLAGS = -Wall -std=c++17

SRCS=server.cpp message_interpreter.cpp
OBJS=$(subst .cpp,.o,$(SRCS))
all: serwer

serwer: $(OBJS)
		$(CXX) $(CXXFLAGS) -o serwer $(OBJS) -lstdc++fs

inter.o: message_interpreter.h message_interpreter.cpp

.PHONY: clean
clean:
		$(RM) $(OBJS) serwer
