.PHONY: all

COMPILER = g++ -std=c++11
PNAME = pose
FLAGS = -Wall

all: pose.o depthCamManager.o
	$(COMPILER) pose.o depthCamManager.o $(FLAGS) `pkg-config --cflags --libs opencv` -lrealsense -o $(PNAME)

pose.o: pose.cpp
	$(COMPILER) -c pose.cpp

depthCamManager.o: depthCamManager.cpp depthCamManager.h
	$(COMPILER) -c depthCamManager.cpp

.PHONY: clean
clean:
	rm -f *.o $(PNAME)