COMMON_DIR = ../common

PROGRAM = ucafs
CXX = clang++
CC = clang
CPPFLAGS = -g -std=c++11
LIBS = -L/usr/local/lib -lprotobuf -pthread\
       -Lmbedtls/library -lmbedcrypto\
       -lglog\
       -luuid
INCFLAGS = -Imbedtls/include -I$(COMMON_DIR) -I/usr/local/include

OBJS = dirnode.o\
       encode.o\
       dirops.o\
       dnode.pb.o

TESTS := test_dnode test_dops
