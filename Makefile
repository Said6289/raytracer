.PHONY: all
all:
	$(CXX) -std=c++11 -g -O2 -o ray -lm ray.cpp
