CXX = g++

CXXFLAGS = -Wall -Wextra -std=c++11

LDLIBS = ../../libs/lib/libzmq.a -lrt ../../libs/lib/libconfig++.a ../../libs/lib/libprotobuf.a ../../libs/lib/libsnappy.a


SERVICE_INCLUDE_DIR = -I../../include/service -I../../libs/include -I../../include/bwt -I../../submodules/rocksdb/include

SERVICE_LDFLAGS = ../../libs/lib/libbwt.a ../../libs/lib/librocksdb.a -lpthread -lz -lbz2

SERVICE_SOURCES = readserver.pb.cc ksw.c service.cpp

SERVICE_TARGET = service


SERVICE_SAMPLES_INCLUDE_DIR = -I../../include/service -I../../libs/include -I../../submodules/rocksdb/include

SERVICE_SAMPLES_LDFLAGS = ../../libs/lib/librocksdb.a -lpthread -lz -lbz2

SERVICE_SAMPLES_SOURCES = readserver.pb.cc service_samples.cpp

SERVICE_SAMPLES_TARGET = service_samples


SERVER_INCLUDE_DIR = -I../../include/service -I../../libs/include -I../../submodules/mongoose

SERVER_LDFLAGS = -lpthread

SERVER_SOURCES = readserver.pb.cc ../../submodules/mongoose/mongoose.c server.cpp

SERVER_TARGET = server


INSTALL_DIR = ../../libs/bin

.PHONY: all clean install

all: $(SERVICE_TARGET) $(SERVER_TARGET) $(SERVICE_SAMPLES_TARGET)

$(SERVICE_TARGET):
	$(CXX) $(CXXFLAGS) $(SERVICE_INCLUDE_DIR) -o $(SERVICE_TARGET) $(SERVICE_SOURCES) $(SERVICE_LDFLAGS) $(LDLIBS)

$(SERVER_TARGET):
	$(CXX) $(CXXFLAGS) $(SERVER_INCLUDE_DIR) -o $(SERVER_TARGET) $(SERVER_SOURCES) $(SERVER_LDFLAGS) $(LDLIBS)

$(SERVICE_SAMPLES_TARGET):
	$(CXX) $(CXXFLAGS) $(SERVICE_SAMPLES_INCLUDE_DIR) -o $(SERVICE_SAMPLES_TARGET) $(SERVICE_SAMPLES_SOURCES) $(SERVICE_SAMPLES_LDFLAGS) $(LDLIBS)

clean:
	$(RM) $(SERVICE_TARGET)
	$(RM) $(INSTALL_DIR)/$(SERVICE_TARGET)
	$(RM) $(SERVICE_SAMPLES_TARGET)
	$(RM) $(INSTALL_DIR)/$(SERVICE_SAMPLES_TARGET)
	$(RM) $(SERVER_TARGET)
	$(RM) $(INSTALL_DIR)/$(SERVER_TARGET)

install:
	cp $(SERVICE_TARGET) $(INSTALL_DIR)
	cp $(SERVICE_SAMPLES_TARGET) $(INSTALL_DIR)
	cp $(SERVER_TARGET) $(INSTALL_DIR)
