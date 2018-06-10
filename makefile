CC = clang++
CPPFLAGS = -x c++ -Wall -Wextra -g -march=native -std=c++14 -stdlib=libc++ -O3 -I/usr/local/include -I./src

LDFLAGS = -L/usr/local/Cellar/grpc/1.0.1/lib -L/usr/local/Cellar/google-protobuf/3.0.2/lib -lgrpc++ -lprotobuf.10 -stdlib=libc++

API_SRC_DIR = ./src/api
API_OBJ_DIR = ./obj/api
API_SRC_FILES = $(wildcard $(API_SRC_DIR)/*.cc)
API_OBJ_FILES = $(patsubst $(API_SRC_DIR)/%.cc,$(API_OBJ_DIR)/%.o,$(API_SRC_FILES))

SRC_DIR = ./src/WorldGenerator
OBJ_DIR = ./obj/WorldGenerator
SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))

.DEFAULT_GOAL := setup

debug: $(API_OBJ_FILES) $(OBJ_FILES)
	$(CC) $(LDFLAGS) -o $@ $^

$(API_OBJ_DIR)/%.o: $(API_SRC_DIR)/%.cc
	$(CC) $(CPPFLAGS) -c $^ -o $@ 

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CC) $(CPPFLAGS) -c $^ -o $@


clean:
	rm obj/api/*
	rm obj/WorldGenerator/*

setup:
	[[ -d obj ]] || mkdir obj
	[[ -d obj/api ]] || mkdir obj/api
	[[ -d obj/WorldGenerator ]] || mkdir obj/WorldGenerator