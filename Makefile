RM = rm -f
CXX = g++
CXXFLAGS = -std=c++17 -O2 -pedantic -Wall -Wextra -Werror -Wshadow -Wconversion
SRCS = server.cpp message_interpreter.cpp
OBJS = $(subst .cpp,.o,$(SRCS))

all: serwer

serwer: $(OBJS)
	$(CXX) $(CXXFLAGS) -o serwer $(OBJS) -lstdc++fs

inter.o: message_interpreter.h message_interpreter.cpp

.PHONY: clean
clean:
		$(RM) $(OBJS) serwer
