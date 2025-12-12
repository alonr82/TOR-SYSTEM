CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c11 -g

INCLUDES = -Icommon -Iserver/header -Iserver/configurations/header -Iclient -Irelay

SRC_SERVER = \
    server/src/main_test.c \
    server/src/server.c \
    server/src/relay_manager.c \
    server/configurations/src/dir_server_config.c \
    server/configurations/src/relay_server_config.c


OBJ_SERVER = $(SRC_SERVER:.c=.o)
SERVER     = tor_server

DIR_CFG ?= dir.cfg

all: $(SERVER)

$(SERVER): $(OBJ_SERVER)
	$(CC) $(CFLAGS) $(OBJ_SERVER) -o $(SERVER)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

run10: $(SERVER)
	@printf "ip=127.0.0.1\nport=9000\n" > $(DIR_CFG)
	@for p in 8001 8002 8003 8004 8005 8006 8007 8008 8009 8010 ; do \
		echo "----- relay port $$p -----" ; \
		./$(SERVER) -p $$p $(DIR_CFG) ; \
	done

valgrind10: $(SERVER)
	@printf "ip=127.0.0.1\nport=9000\n" > $(DIR_CFG)
	@for p in 8001 8002 8003 8004 8005 8006 8007 8008 8009 8010 ; do \
		echo "----- relay port $$p -----" ; \
		valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(SERVER) -p $$p $(DIR_CFG) ; \
	done

clean:
	rm -f $(OBJ_SERVER) $(SERVER) $(DIR_CFG)

.PHONY: all clean run10 valgrind10
