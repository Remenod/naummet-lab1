CC = gcc
CXX = g++
CFLAGS = -O2 -Wall -Wextra
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra

TARGET = app

C_SRC = tinyexpr.c
CPP_SRC = main.cpp

OBJS = $(C_SRC:.c=.o) $(CPP_SRC:.cpp=.o)

all: $(TARGET)

tinyexpr.o: tinyexpr.c
	$(CC) $(CFLAGS) -Wno-array-bounds -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

INPUT ?= input.txt

run_file: $(TARGET)
	./$(TARGET) < $(INPUT)
	
run_var12: $(TARGET)
	printf "sin(x) - 0.5*cos(x^2)\n-6.5 6.5\n" | ./app

.PHONY: all clean run
