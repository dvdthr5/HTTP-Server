CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -Isrc
LDFLAGS = -pthread

SRC_DIR = src
OBJ_DIR = obj


TARGET = httpserver

all: $(TARGET)

$(TARGET): \
    $(OBJ_DIR)/main.o \
    $(OBJ_DIR)/server.o \
    $(OBJ_DIR)/connection.o \
    $(OBJ_DIR)/http.o \
    $(OBJ_DIR)/dispatcher.o \
    $(OBJ_DIR)/file.o \
    $(OBJ_DIR)/log.o \
    $(OBJ_DIR)/parallel.o
	$(CC) $(CFLAGS) $^ -o $(TARGET) $(LDFLAGS)

$(OBJ_DIR)/main.o: $(SRC_DIR)/main.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/main.c -o $(OBJ_DIR)/main.o

$(OBJ_DIR)/server.o: $(SRC_DIR)/server.c $(SRC_DIR)/server.h
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/server.c -o $(OBJ_DIR)/server.o

$(OBJ_DIR)/connection.o: $(SRC_DIR)/connection.c $(SRC_DIR)/connection.h
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/connection.c -o $(OBJ_DIR)/connection.o

$(OBJ_DIR)/http.o: $(SRC_DIR)/http.c $(SRC_DIR)/http.h
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/http.c -o $(OBJ_DIR)/http.o

$(OBJ_DIR)/dispatcher.o: $(SRC_DIR)/dispatcher.c $(SRC_DIR)/dispatcher.h
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/dispatcher.c -o $(OBJ_DIR)/dispatcher.o

$(OBJ_DIR)/file.o: $(SRC_DIR)/file.c $(SRC_DIR)/file.h
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/file.c -o $(OBJ_DIR)/file.o

$(OBJ_DIR)/log.o: $(SRC_DIR)/log.c $(SRC_DIR)/log.h
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/log.c -o $(OBJ_DIR)/log.o

$(OBJ_DIR)/parallel.o: $(SRC_DIR)/parallel.c $(SRC_DIR)/parallel.h
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/parallel.c -o $(OBJ_DIR)/parallel.o

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
