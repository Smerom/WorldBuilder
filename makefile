SHELL = /bin/bash
CXX = clang++-6.0
CPPFLAGS = -x c++ -Wall -Wextra -Wno-unused-parameter -g -march=native -std=c++14 -stdlib=libc++ -I/usr/local/include -I./src $(shell pkg-config --cflags protobuf grpc)
OP=-O0

LDFLAGS = -L/usr/local/lib -stdlib=libc++ -lc++ -g -lc++abi $(shell pkg-config --libs protobuf grpc++)

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
	$(CXX) $(LDFLAGS) -o $@ $^

$(API_OBJ_DIR)/%.o: $(API_SRC_DIR)/%.cc
	$(CXX) $(CPPFLAGS) $(OP) -c $^ -o $@ 

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CPPFLAGS) $(OP) -c $^ -o $@

protoc:
	protoc --cpp_out=src/api -Isrc/proto src/proto/Basic.proto
	protoc --grpc_out=src/api -Isrc/proto --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` src/proto/Basic.proto

clean:
	rm obj/api/*
	rm obj/WorldGenerator/*

setup:
	$(shell [[ -d src/api ]] || mkdir src/api)
	$(shell [[ -d obj ]] || mkdir obj)
	$(shell [[ -d obj/api ]] || mkdir obj/api)
	$(shell [[ -d obj/WorldGenerator ]] || mkdir obj/WorldGenerator)