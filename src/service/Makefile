CXX = g++

CXXFLAGS = -Wall -Wextra -std=c++11

LINK_LIBRARY = -L../../libs/lib


SERVICE_INCLUDE_DIR = -I../../include/service -I../../libs/include -I../../include/bwt -I../../submodules/rocksdb/include

SERVICE_LDFLAGS = -lzmq -lconfig++ -lprotobuf -lbwt -lrocksdb -lpthread -lz -lbz2

SERVICE_SOURCES = readserver.pb.cc ksw.c service.cpp

SERVICE_TARGET = service


SERVER_INCLUDE_DIR = -I../../include/service -I../../libs/include -I../../submodules/mongoose

SERVER_LDFLAGS = -lzmq -lconfig++ -lprotobuf -lpthread

SERVER_SOURCES = readserver.pb.cc ../../submodules/mongoose/mongoose.c server.cpp

SERVER_TARGET = server


INSTALL_DIR = ../../libs/bin

.PHONY: all clean install

all: $(SERVICE_TARGET) $(SERVER_TARGET) 

$(SERVICE_TARGET):
	$(CXX) $(CXXFLAGS) $(SERVICE_INCLUDE_DIR) -o $(SERVICE_TARGET) $(SERVICE_SOURCES) $(LINK_LIBRARY) $(SERVICE_LDFLAGS)

$(SERVER_TARGET):
	$(CXX) $(CXXFLAGS) $(SERVER_INCLUDE_DIR) -o $(SERVER_TARGET) $(SERVER_SOURCES) $(LINK_LIBRARY) $(SERVER_LDFLAGS)

clean:
	$(RM) $(SERVICE_TARGET)
	$(RM) $(INSTALL_DIR)/$(SERVICE_TARGET)
	$(RM) $(SERVER_TARGET)
	$(RM) $(INSTALL_DIR)/$(SERVER_TARGET)

install:
	cp $(SERVICE_TARGET) $(INSTALL_DIR)
	cp $(SERVER_TARGET) $(INSTALL_DIR)
