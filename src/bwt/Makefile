CXX = g++

CXXFLAGS = -Wall -Wextra -std=c++11 -fPIC -I../../include/bwt

ARFLAGS = rvs

SOURCES = $(shell echo *.cpp)

OBJECTS = $(SOURCES:.cpp=.o)

TARGET = libbwt.a

INSTALL_DIR = ../../libs/lib

.PHONY: all clean install

all: $(OBJECTS)
	$(AR) $(ARFLAGS) $(TARGET) $(OBJECTS)

clean:
	$(RM) $(TARGET) $(OBJECTS)
	$(RM) $(INSTALL_DIR)/$(TARGET)

install:
	cp $(TARGET) $(INSTALL_DIR)
