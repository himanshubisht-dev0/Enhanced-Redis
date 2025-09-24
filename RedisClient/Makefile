#compiler
CXX=g++
CXXFLAGS=-Wall -Wextra -std=c++17

#Directories
SRC_DIR=src
BUILD_DIR=build
BIN_DIR=bin
INC_DIR=include

#Find all .cpp files in the client directory
SRC:=$(wildcard $(SRC_DIR)/*.cpp)
OBJS:=$(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC))

#Output binary
TARGET=$(BIN_DIR)/my_redis_cli

#default rule
all:$(TARGET)

#Create build and bin directories if they don't exist
$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

#compile each .cpp file into .o file
$(BUILD_DIR)/%.o:$(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c $< -o $@
#Link the object files to create the executable
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)
#clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
#Rebuild from scratch
rebuild: clean all
#run the complied binary
run:all
	./$(TARGET)