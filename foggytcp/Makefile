TOP_DIR = .
INC_DIR = $(TOP_DIR)/inc
SRC_DIR = $(TOP_DIR)/src
BUILD_DIR = $(TOP_DIR)/build
CXX=g++
ASAN = -fsanitize=undefined -fno-omit-frame-pointer 
#-fsanitize=undefined
FLAGS = -pthread -fPIC -g -ggdb -pedantic -Wall -Wextra -Wno-missing-field-initializers -DDEBUG -I$(INC_DIR) $(ASAN) 
FLAGS_Test = -pthread -fPIC -g -ggdb -pedantic -Wall -Wextra -Wno-missing-field-initializers -DDEBUG -I$(INC_DIR)
SYSTEM_OBJS = $(BUILD_DIR)/system_tcp.o
FOGGY_OBJS = $(BUILD_DIR)/foggy_tcp.o $(BUILD_DIR)/foggy_backend.o $(BUILD_DIR)/foggy_packet.o $(BUILD_DIR)/foggy_function.o

foggy: server-foggy client-foggy

system: server-system client-system

test: server_test client_test

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	$(CXX) $(FLAGS) -c -o $@ $<

server-foggy: $(FOGGY_OBJS) $(SRC_DIR)/server.cc
	$(CXX) $(FLAGS) $(SRC_DIR)/server.cc -o server $(FOGGY_OBJS)

client-foggy: $(FOGGY_OBJS) $(SRC_DIR)/client.cc
	$(CXX) $(FLAGS) $(SRC_DIR)/client.cc -o client $(FOGGY_OBJS)

server-system: $(SYSTEM_OBJS) $(SRC_DIR)/server.cc
	$(CXX) $(FLAGS) $(SRC_DIR)/server.cc -o server $(SYSTEM_OBJS)

client-system: $(SYSTEM_OBJS) $(SRC_DIR)/client.cc
	$(CXX) $(FLAGS) $(SRC_DIR)/client.cc -o client $(SYSTEM_OBJS)

server_test: $(FOGGY_OBJS) $(SRC_DIR)/server_test.cc
	$(CXX) $(FLAGS) $(SRC_DIR)/server_test.cc -o server_test $(FOGGY_OBJS)

client_test: $(FOGGY_OBJS) $(SRC_DIR)/client_test.cc
	$(CXX) $(FLAGS) $(SRC_DIR)/client_test.cc -o client_test $(FOGGY_OBJS)

format:
	pre-commit run --all-files

clean:
	rm -f $(BUILD_DIR)/*.o client server server_test client_test
