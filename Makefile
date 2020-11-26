APP := Node
APP2 := Wc
APP3 := Wr
INC_DIR := ./inc
CFLAGS := -g -Wall -std=c++11 -I$(INC_DIR)
LIBS := -lpthread
SRC_FILES := src/Node.cpp src/Messages.cpp src/Member.cpp src/UdpSocket.cpp src/Threads.cpp src/Utils.cpp src/Logger.cpp src/TcpSocket.cpp src/TestBench.cpp src/main.cpp src/HashRing.cpp src/FileObject.cpp
SRC_FILES2 := mappers/wc.cpp src/Utils.cpp
SRC_FILES3 := mappers/wr.cpp src/Utils.cpp
.PHONY: clean

all: clean app map reduce

reduce:
	$(CXX) -o $(APP3) $(SRC_FILES3) $(CFLAGS) $(LIBS)
map:
	$(CXX) -o $(APP2) $(SRC_FILES2) $(CFLAGS) $(LIBS)
app:
	$(CXX) -o $(APP) $(SRC_FILES) $(CFLAGS) $(LIBS)

clean:
	$(RM) -f $(APP) *.o
	$(RM) -f $(APP2) *.o
	$(RM) -f $(APP3) *.o
