CXX = g++

CXXFLAGS = -Wall -Wextra -std=c++11 -I../../libs/include -I../../include/util -I../../include/bwt -I../../submodules/rocksdb/include

LDFLAGS = -L../../libs/lib

LDLIBS = -lbwt -lrocksdb -lpthread

INSTALL_DIR = ../../libs/bin

SOURCES = $(shell echo *.cpp)

OBJECTS = $(SOURCES:.cpp=)

.PHONY: all clean install

all: $(OBJECTS)

clean:
	$(RM) $(OBJECTS)
	$(RM) $(INSTALL_DIR)/$(OBJECTS)

install:
	mv $(OBJECTS) $(INSTALL_DIR)
