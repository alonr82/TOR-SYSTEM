CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c11 -g
INCLUDES = -Icommon -Iserver/header -Iclient -Irelay

SRC_SERVER = server/src/main_test.c server/src/server.c server/src/relay_manager.c
OBJ_SERVER = $(SRC_SERVER:.c=.o)
SERVER     = tor_server

all: $(SERVER)

$(SERVER): $(OBJ_SERVER)
	$(CC) $(CFLAGS) $(OBJ_SERVER) -o $(SERVER)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

run: $(SERVER)
	./$(SERVER)

valgrind: $(SERVER)
	valgrind --leak-check=full \
	         --show-leak-kinds=all \
	         --track-origins=yes \
	         ./$(SERVER)

clean:
	rm -f $(OBJ_SERVER) $(SERVER)

.PHONY: all clean run valgrind
