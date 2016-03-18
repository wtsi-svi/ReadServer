CXX = g++

CXXFLAGS = -Wall -Wextra -std=c++11 -fPIC -I../../include/bwt

LDFLAGS = -shared

SOURCES = $(shell echo *.cpp)

OBJECTS = $(SOURCES:.cpp=.o)

TARGET = libbwt.so

INSTALL_DIR = ../../libs/lib

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(OBJECTS)
	$(RM) $(INSTALL_DIR)/$(TARGET)

install:
	mv $(TARGET) $(INSTALL_DIR)
