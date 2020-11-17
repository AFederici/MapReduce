APP := Node
INC_DIR := ./inc
CFLAGS := -g -Wall -std=c++11 -I$(INC_DIR)
LIBS := -lpthread
SRC_FILES := src/Node.cpp src/Messages.cpp src/Member.cpp src/UdpSocket.cpp src/Threads.cpp src/Utils.cpp src/Logger.cpp src/TcpSocket.cpp src/TestBench.cpp src/main.cpp src/HashRing.cpp src/FileObject.cpp

.PHONY: clean

all: clean app

app:
	$(CXX) -o $(APP) $(SRC_FILES) $(CFLAGS) $(LIBS)

clean:
	$(RM) -f $(APP) *.o
