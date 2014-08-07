VERSION = 0.01
CC      = /usr/bin/g++
CFLAGS  = -std=gnu++0x -Wall -Wno-unused-function -g -O3 -D_REENTRANT -DVERSION=\"$(VERSION)\"
LDFLAGS = -lrt -lpthread -lportaudio -lboost_system

OBJ_SERVER = blocking_tcp_echo_server.o
BIN_SERVER = server
OBJ_CLIENT = client.o stream.o chunk.o
BIN_CLIENT = client
OBJ = $(OBJ_SERVER) $(OBJ_CLIENT)

all:	server client

server: $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN_SERVER) $(OBJ_SERVER) $(LDFLAGS)

client: $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN_CLIENT) $(OBJ_CLIENT) $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf $(BIN_SERVER) $(BIN_CLIENT) $(OBJ) $(OBJ_SERVER) $(OBJ_CLIENT)

